#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# $env:CRYPTOGRAPHY_OPENSSL_NO_LEGACY=1

import asyncio
import logging
import socket
import websockets
import json
# =================================================================
# 全局配置
# =================================================================
logging.basicConfig(level=logging.INFO, format='%(asctime)s %(levelname)s - %(message)s')
WS_HOST        = "0.0.0.0"
WS_PORT        = 8080
UDP_IP         = "0.0.0.0"
UDP_PORT       = 8888
BROADCAST_PORT = 9999

# ———— 可选固定 IP 配置 ————
FIXED_HOST_IP  = None       # 手动指定用于广播的服务器 IP

# ———— 命令发送配置 ————
CMD_PORT       = 9999       # 板子监听命令的端口
ACK_TIMEOUT    = 1.0        # 等待 ACK 最长秒数
MAX_RETRIES    = 3          # 最多重试次数

# ======= 全局状态 ========
CONNECTED      = set()      # 活跃的 WebSocket 客户端
board_addr     = None       # 板子的 (ip, port)
first_frame_event: asyncio.Event
command_queue: asyncio.Queue  # 存 bytes 命令

# =================================================================
# 获取本机局域网 IP
# =================================================================
def get_local_ip():
    if FIXED_HOST_IP:
        logging.info(f"⚙️ 使用固定主机 IP: {FIXED_HOST_IP}")
        return FIXED_HOST_IP
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        logging.info(f"⚙️ 自动获取主机 IP: {ip}")
    except Exception:
        ip = "127.0.0.1"
        logging.warning("⚠️ 获取主机 IP 失败，使用 127.0.0.1")
    finally:
        s.close()
    return ip

# =================================================================
# 广播服务（局域网发现）
# =================================================================
async def broadcast_presence(host_ip, broadcast_port=BROADCAST_PORT):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    broadcast_address = ("<broadcast>", broadcast_port)
    logging.info(f"📢 广播启动: 向 {broadcast_address} 广播 {host_ip}")
    while True:
        try:
            msg = f"FITNESS_MIRROR_SERVER_AT:{host_ip}:{WS_PORT}".encode('ascii')
            sock.sendto(msg, broadcast_address)
            await asyncio.sleep(5)
        except Exception as e:
            logging.error(f"广播错误: {e}")
            await asyncio.sleep(10)

# =================================================================
# UDP 帧生产者
# =================================================================
async def udp_frame_producer(queue: asyncio.Queue):
    global board_addr
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_PORT))
    sock.settimeout(0.01)
    logging.info(f"🚀 UDP 启动: 监听 {UDP_IP}:{UDP_PORT}")
    jpg_start, jpg_end = b'\xff\xd8', b'\xff\xd9'
    buf, first = b"", True
    while True:
        try:
            packet, addr = sock.recvfrom(65536)
            buf += packet
            if board_addr is None:
                board_addr = (addr[0], CMD_PORT)
                logging.info(f"🔗 发现板子地址: {board_addr}")
        except socket.timeout:
            await asyncio.sleep(0.001)
            continue
        i = buf.find(jpg_start)
        if i != -1:
            j = buf.find(jpg_end, i+2)
            if j != -1:
                j += 2
                frame = buf[i:j]
                buf = buf[j:]
                if first:
                    first = False
                    first_frame_event.set()
                    logging.info("✅ 首帧接收成功，WS 推送就绪")
                if queue.full():
                    _ = queue.get_nowait()
                await queue.put(frame)

# =================================================================
# 命令发送协程（直接转发收到的 bytes，并校验 ACK0/ACK1/ACK2）
# =================================================================
async def command_sender():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(ACK_TIMEOUT)
    while True:
        data = await command_queue.get()  # data 是 bytes 类型, 如 b"0"、b"1"、b"2"
        if not board_addr:
            logging.warning("⚠️ 板子地址未知，无法下发命令")
            command_queue.task_done()
            continue
        # cmd_str: 数字字符串
        try:
            cmd_str = data.decode('ascii')
        except UnicodeDecodeError:
            cmd_str = ''
        expected_ack = f"ACK{cmd_str}".encode('ascii')

        try:
            while True:
                sock.recvfrom(1024)
        except socket.timeout:
            pass

        for attempt in range(1, MAX_RETRIES+1):
            logging.info(f"🔀 发送命令 {cmd_str} 到 {board_addr} (尝试 {attempt})")
            sock.sendto(data, board_addr)
             # 等待一会儿让板子回包
            await asyncio.sleep(0.05)
            try:
                resp, _ = sock.recvfrom(1024)
                if resp == expected_ack:
                    logging.info(f"📨 收到 ACK: {resp.decode('ascii')} (尝试 {attempt})")
                    break
                else:
                    logging.warning(f"⚠️ 收到非预期响应 {resp!r}, 期望 {expected_ack!r}")
            except socket.timeout:
                logging.warning(f"⚠️ 无 ACK，重试 {attempt}")
        else:
            final = locals().get('resp', None)
            logging.error(f"❌ 命令 '{cmd_str}' 未被确认，最终响应 {final!r}")
        command_queue.task_done()

# =================================================================
# WebSocket 处理（只接收 JSON 中的 command 字段作为纯数字命令）
# =================================================================
async def ws_handler(ws: websockets.WebSocketServerProtocol):
    global command_queue
    CONNECTED.add(ws)
    logging.info(f"🔗 WS 客户端连接: {ws.remote_address}")
    try:
        async for msg in ws:
            data_bytes = None
            # 解析 JSON，只读取 "command"
            try:
                obj = json.loads(msg)
                cmd = obj.get('command')
                if isinstance(cmd, int) or (isinstance(cmd, str) and cmd.isdigit()):
                    data_bytes = str(cmd).encode('ascii')
                    logging.info(f"接收到 JSON 命令，command -> 纯数字: {cmd}")
                else:
                    logging.warning(f"⚠️ 忽略无效或缺失的 command 字段: {obj}")
                    continue
            except json.JSONDecodeError:
                logging.warning(f"⚠️ 无效 JSON，忽略消息: {msg}")
                continue

            # 放入命令队列并回馈客户端
            await command_queue.put(data_bytes)
            await ws.send(f"Server: 下发纯数字命令 {data_bytes.decode('ascii')}")
    finally:
        CONNECTED.remove(ws)
        logging.info(f"🔌 WS 客户端断开: {ws.remote_address}")

# =================================================================
# 帧广播协程
# =================================================================
async def broadcaster(queue: asyncio.Queue):
    logging.info("📢 广播协程启动")
    while True:
        frame = await queue.get()
        if CONNECTED:
            await asyncio.gather(*[ws.send(frame) for ws in CONNECTED], return_exceptions=True)
        queue.task_done()

# =================================================================
# 主入口
# =================================================================
async def main():
    global first_frame_event, command_queue
    first_frame_event = asyncio.Event()
    command_queue     = asyncio.Queue()
    frame_queue       = asyncio.Queue(maxsize=10)
    host_ip = get_local_ip()
    asyncio.create_task(udp_frame_producer(frame_queue))
    asyncio.create_task(broadcaster(frame_queue))
    asyncio.create_task(broadcast_presence(host_ip))
    asyncio.create_task(command_sender())
    ws_srv = await websockets.serve(ws_handler, WS_HOST, WS_PORT)
    logging.info(f"✅ WS 服务启动: ws://{WS_HOST}:{WS_PORT} (本机 IP: {host_ip})")
    await ws_srv.wait_closed()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logging.info("🛑 服务中止，退出")
