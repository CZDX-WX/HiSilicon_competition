import { defineStore } from 'pinia';
import { ref } from 'vue';
import type { SystemStatus } from './workoutStore';
import type { HisiCommandKey } from '@/config/commandMap';

// 定义一条日志的数据结构
export interface LogEntry {
  timestamp: string;
  status: SystemStatus;
  commandKey: HisiCommandKey;
  commandValue: number;
  voiceText: string;
}

export const useLogStore = defineStore('log', () => {
  const logs = ref<LogEntry[]>([]);
  const MAX_LOGS = 100; // 最多保留100条日志，防止内存溢出

  /**
   * 添加一条新的日志记录
   */
  const addLog = (entry: Omit<LogEntry, 'timestamp'>) => {
    const newLog: LogEntry = {
      timestamp: new Date().toLocaleTimeString(),
      ...entry,
    };
    logs.value.push(newLog);

    // 如果日志超过最大数量，则从头部删除旧日志
    if (logs.value.length > MAX_LOGS) {
      logs.value.shift();
    }
  };

  return { logs, addLog };
});
