import { ref } from 'vue';
import { HisiCommand, type HisiCommandKey } from '@/config/commandMap';

class WebSocketService {
  public ws = ref<WebSocket | null>(null);

  /**
   * 【已升级】发送一个预定义的指令
   * @param commandKey - commandMap.ts 中定义的指令的键名
   */
  public sendCommand(commandKey: HisiCommandKey) {
    if (this.ws.value && this.ws.value.readyState === WebSocket.OPEN) {
      const commandValue = HisiCommand[commandKey];
      const message = JSON.stringify({ command: commandValue });
      this.ws.value.send(message);
      console.log(`[WebSocketService] 已发送指令: ${commandKey} -> ${message}`);
    } else {
      console.error(`WebSocket 未连接，指令 ${commandKey} 发送失败。`);
    }
  }
}

export const webSocketService = new WebSocketService();
