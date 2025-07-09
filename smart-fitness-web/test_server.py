import asyncio
import json
import logging
import websockets

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(message)s')

async def command_echo_handler(websocket):
    """
    一个简单的回声服务器，它会打印收到的每一条消息。
    """
    logging.info(f"✅ 前端测试客户端已连接: {websocket.remote_address}")
    try:
        async for message in websocket:
            try:
                # 尝试解析为JSON，如果失败则直接打印原始消息
                data = json.loads(message)
                logging.info(f"✅ [TEST SERVER] 收到指令: {data}")
            except json.JSONDecodeError:
                logging.info(f"✅ [TEST SERVER] 收到纯文本指令: {message}")
    except websockets.exceptions.ConnectionClosed:
        logging.info(f"❌ 客户端 {websocket.remote_address} 连接已断开。")

async def main():
    async with websockets.serve(command_echo_handler, "0.0.0.0", 8080) as server:
        logging.info(f"✅ 指令测试服务器已启动于 ws://0.0.0.0:8080")
        await server.wait_closed()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n🛑 服务器被手动中断。")