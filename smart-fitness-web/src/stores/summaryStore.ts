import { defineStore } from 'pinia';
import { ref } from 'vue';
import type { WorkoutRecord } from '@/types/fitness.d';

export const useSummaryStore = defineStore('summary', () => {
  const showSummary = ref(false);
  // state 现在持有的是包含详细 repDetails 的完整 WorkoutRecord
  const summaryData = ref<WorkoutRecord | null>(null);

  /**
   * 显示总结报告弹窗
   * @param data - 包含所有详细数据的完整训练记录
   */
  const show = (data: WorkoutRecord) => {
    summaryData.value = data;
    showSummary.value = true;
    console.log("准备显示总结报告，数据:", data);
  };

  /**
   * 隐藏总结报告弹窗
   */
  const hide = () => {
    showSummary.value = false;
    summaryData.value = null;
  };

  return { showSummary, summaryData, show, hide };
});
