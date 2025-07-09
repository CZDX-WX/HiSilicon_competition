import { defineStore } from 'pinia';
import { ref, watch } from 'vue';
import type { WorkoutRecord } from '@/types/fitness.d';

const STORAGE_KEY = 'workoutHistory';

export const useHistoryStore = defineStore('history', () => {
  // 从 localStorage 初始化 state
  const historyList = ref<WorkoutRecord[]>(
    JSON.parse(localStorage.getItem(STORAGE_KEY) || '[]')
  );

  // Action: 添加一条记录
  const addRecord = (record: WorkoutRecord) => {
    historyList.value.unshift(record); // unshift 添加到数组开头
  };

  // Action: 清空历史记录
  const clearHistory = () => {
    historyList.value = [];
  };

  // 使用 watch 来自动将 state 的变化同步到 localStorage
  watch(
    historyList,
    (newList) => {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(newList));
    },
    { deep: true } // deep watch 确保数组内部对象变化也能被检测到
  );

  return { historyList, addRecord, clearHistory };
});
