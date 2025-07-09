#!/usr/bin/env python
# -*- coding: utf-8 -*-
import asyncio
import json
import logging
import os
import socket
import time
from fractions import Fraction

import cv2
import numpy as np
import janus
from aiohttp import web
import aiohttp_cors
from aiortc import MediaStreamTrack, RTCPeerConnection, RTCSessionDescription
from av import VideoFrame

# =================================================================
# 全局资源区
# =================================================================
ROOT = os.path.dirname(__file__)
pcs = set()

# =================================================================
# 2. 新增：辅助函数，用于自动获取本机在局域网中的IP地址
# =================================================================
def get_local_ip():
    """
    查询本机在局域网中的IP地址
    """
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # 不需要真正发送数据，只是为了触发系统选择一个可用的网络接口
        s.connect(('8.8.8.8', 80))
        ip = s.getsockname()[0]
    except Exception:
        ip = '127.0.0.1' # 如果无法连接外网，则返回本地地址
    finally:
        s.close()
    return ip


# =================================================================
# 3. 新增：UDP广播任务，作为“灯塔”
# =================================================================
async def broadcast_presence(host_ip, broadcast_port=9999):
    """
    作为一个异步任务，在后台持续广播服务器的存在
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    broadcast_address = ('<broadcast>', broadcast_port)

    logging.info(f"📢 [广播服务] 已启动，将本机IP {host_ip} 广播到端口 {broadcast_port}...")

    while True:
        try:
            message = f"FITNESS_MIRROR_SERVER_AT:{host_ip}".encode('utf-8')
            sock.sendto(message, broadcast_address)
            # 日志：方便调试，可以取消注释下面这行来观察广播是否在持续发送
            # logging.info(f"📢 [广播服务] 已发送广播: {message.decode()}")
            await asyncio.sleep(5)  # 每5秒广播一次
        except Exception as e:
            logging.error(f"💥 [广播服务] 发生错误: {e}")
            await asyncio.sleep(10)  # 发生错误后等待更长时间


# =================================================================
# UDP推流接收逻辑
# =================================================================
async def udp_video_receiver(queue: janus.Queue, ready_event: asyncio.Event, udp_ip="0.0.0.0", udp_port=8888):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((udp_ip, udp_port))
    sock.settimeout(0.01)
    logging.info(f"🚀 异步UDP视频接收器启动 | 正在监听 {udp_ip}:{udp_port}...")
    logging.info("🚦 WebRTC服务将等待首次数据到达后再接受连接。")

    jpg_start = b'\xff\xd8'
    jpg_end = b'\xff\xd9'
    data_buffer = b""

    while True:
        try:
            packet, _ = sock.recvfrom(65536)
            data_buffer += packet
        except socket.timeout:
            await asyncio.sleep(0.001)
            continue
        except Exception as e:
            logging.warning(f"UDP接收错误: {e}")
            continue

        start = data_buffer.find(jpg_start)
        if start != -1:
            end = data_buffer.find(jpg_end, start)
            if end != -1:
                end += 2
                frame_data = data_buffer[start:end]
                data_buffer = data_buffer[end:]

                if queue.sync_q.full():
                    try:
                        queue.sync_q.get_nowait()
                    except Exception:
                        pass
                queue.sync_q.put(frame_data)

                # --- 修复2：重新加入“绿灯”信号逻辑 ---
                if not ready_event.is_set():
                    logging.info("✅ 首次接收到有效视频帧，WebRTC服务现已开放连接！")
                    ready_event.set()
                # # 通知 ready
                # app = queue._loop._current_handle.app  # hack to get app? Instead, pass event separately


# =================================================================
# WebRTC 视频轨道
# =================================================================
class UdpVideoStreamTrack(MediaStreamTrack):
    kind = "video"

    def __init__(self, queue: janus.Queue):
        super().__init__()
        self.queue = queue
        # 定义我们希望输出给浏览器的标准视频尺寸
        self.TARGET_WIDTH = 1280
        self.TARGET_HEIGHT = 720
        self._start_time = time.time()

    async def recv(self):
        frame_data = await self.queue.async_q.get()

        np_arr = np.frombuffer(frame_data, dtype=np.uint8)
        bgr = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)
        if bgr is None:
            logging.warning("解码 JPEG 失败，跳过此帧")
            return await self.recv()

        h, w = bgr.shape[:2]
        bgr = cv2.resize(bgr, (w // 2, h // 2), interpolation=cv2.INTER_LINEAR)

        elapsed = time.time() - self._start_time
        pts = int(elapsed * 90000)

        video_frame = VideoFrame.from_ndarray(bgr, format="bgr24")
        video_frame = video_frame.reformat(
            width=bgr.shape[1],
            height=bgr.shape[0],
            format="yuv420p"
        )
        video_frame.pts = pts
        video_frame.time_base = Fraction(1, 90000)

        return video_frame

# =================================================================
# WebRTC 信令处理
# =================================================================
async def offer(request):
    # --- 修复3：从正确的应用上下文中获取资源 ---
    udp_ready_event = request.app['udp_source_ready']
    frame_queue = request.app['frame_queue']
    try:
        await asyncio.wait_for(udp_ready_event.wait(), timeout=15.0)
    except asyncio.TimeoutError:
        logging.warning("等待视频源超时 (15秒)，拒绝 WebRTC 连接。")
        return web.Response(status=503, text="Service Unavailable: Video source not ready.")

    logging.info("✅ 视频源已就绪，开始处理 WebRTC Offer。")
    params = await request.json()
    offer = RTCSessionDescription(sdp=params["sdp"], type=params["type"])
    pc = RTCPeerConnection()
    pcs.add(pc)

    @pc.on("connectionstatechange")
    async def on_connectionstatechange():
        logging.info(f"Connection state is {pc.connectionState}")
        if pc.connectionState in ("failed", "closed", "disconnected"):
            await pc.close()
            pcs.discard(pc)

    video_track = UdpVideoStreamTrack(frame_queue)
    pc.addTrack(video_track)

    await pc.setRemoteDescription(offer)
    answer = await pc.createAnswer()
    await pc.setLocalDescription(answer)

    return web.json_response({"sdp": pc.localDescription.sdp, "type": pc.localDescription.type})

# =================================================================
# aiohttp 应用启动与清理
# =================================================================
async def on_shutdown(app):
    coros = [pc.close() for pc in pcs]
    await asyncio.gather(*coros)
    pcs.clear()


# =================================================================
# 7. 【核心修改】在服务器启动时，一并启动广播任务
# =================================================================
async def start_background_tasks(app):
    # --- 【核心修复 ①】 ---
    # 在这个 on_startup 触发的函数里创建需要事件循环的资源
    app['frame_queue'] = janus.Queue()
    app['udp_source_ready'] = asyncio.Event()

    logging.info("后台任务启动：正在创建UDP接收器和广播器...")

    # 现在从 app 上下文中获取资源并传递给任务
    app['udp_receiver'] = asyncio.create_task(
        udp_video_receiver(app['frame_queue'], app['udp_source_ready'])
    )
    host_ip = get_local_ip()
    app['udp_broadcaster'] = asyncio.create_task(broadcast_presence(host_ip))


async def cleanup_background_tasks(app):
    logging.info("正在清理后台任务...")
    app['udp_receiver'].cancel()
    app['udp_broadcaster'].cancel()
    await app['udp_receiver']
    await app['udp_broadcaster']


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format='%(asctime)s %(levelname)s - %(message)s')

    app = web.Application()

    app.on_startup.append(start_background_tasks)
    app.on_cleanup.append(cleanup_background_tasks)
    app.router.add_post("/offer", offer)

    # CORS 配置
    cors = aiohttp_cors.setup(app, defaults={
        "*": aiohttp_cors.ResourceOptions(allow_credentials=True, expose_headers="*", allow_headers="*",
                                          allow_methods="*")
    })
    for route in list(app.router.routes()):
        cors.add(route)

    web.run_app(app, host='0.0.0.0', port=8080)