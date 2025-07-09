<script setup lang="ts">
import { useHistoryStore } from '@/stores/historyStore';
import { useSummaryStore } from '@/stores/summaryStore';
import type { WorkoutRecord } from '@/types/fitness.d';
import type { WorkoutType } from '@/services/exerciseService';

const historyStore = useHistoryStore();
const summaryStore = useSummaryStore();

// --- 新增：创建运动ID到中文名称的映射表 ---
const workoutNameMap: Record<WorkoutType, string> = {
  squat: '标准深蹲',
  bicep_curl: '二头弯举',
  lateral_raise: '侧平举',
  overhead_press: '过头推举',
  jumping_jack: '开合跳',
};

const showRecordDetails = (record: WorkoutRecord) => {
  summaryStore.show(record);
};
</script>

<template>
  <div class="history-page">
    <div class="header">
      <h1>训练历史</h1>
      <button @click="historyStore.clearHistory()" v-if="historyStore.historyList.length > 0">
        清空记录
      </button>
    </div>

    <div v-if="historyStore.historyList.length === 0" class="empty-state">
      <p>暂无历史记录，开始一次新的训练吧！</p>
    </div>

    <div v-else class="history-list">
      <div
        v-for="record in historyStore.historyList"
        :key="record.id"
        class="history-card glass-card"
        @click="showRecordDetails(record)"
        tabindex="0"
        @keydown.enter="showRecordDetails(record)"
      >
        <div class="card-header">
          <h3>{{ workoutNameMap[record.workoutType as WorkoutType] || record.workoutType }}</h3>
          <span class="date">{{ record.endDate }}</span>
        </div>
        <div class="stats">
          <div class="stat-item"><span>次数</span><strong>{{ record.totalReps }}</strong></div>
          <div class="stat-item"><span>时长</span><strong>{{ record.totalTime }}</strong></div>
          <div class="stat-item"><span>消耗(千卡)</span><strong>{{ record.totalCalories.toFixed(1) }}</strong></div>
        </div>
        <div class="card-footer">
          <span>查看详情 &rarr;</span>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
/* 样式无需修改 */
.glass-card {
  background: rgba(22, 27, 34, 0.5);
  backdrop-filter: blur(10px);
  border: 1px solid var(--color-border);
  border-radius: 12px;
  transition: all 0.3s ease;
}

.history-page {
  padding: 24px 5vw;
  color: var(--color-text-primary);
}
.header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 24px;
  max-width: 1200px;
  margin-left: auto;
  margin-right: auto;
}
.header button {
  background-color: var(--color-accent-danger);
  color: #fff;
  padding: 8px 16px;
  border:none;
  border-radius: 6px;
  cursor:pointer;
  font-weight: 700;
}
.empty-state {
  text-align: center;
  padding: 40px;
  color: var(--color-text-secondary);
}

.history-list {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(350px, 1fr));
  gap: 20px;
  max-width: 1200px;
  margin: 0 auto;
}

.history-card {
  padding: 24px;
  display: flex;
  flex-direction: column;
  gap: 20px;
  cursor: pointer;
  outline: none;
}

.history-card:hover, .history-card:focus {
  border-color: var(--color-accent-primary);
  transform: translateY(-5px);
  box-shadow: 0 10px 30px rgba(88, 166, 255, 0.1);
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: baseline;
}
.card-header h3 { margin: 0; font-size: 1.5rem; }
.card-header .date { color: var(--color-text-secondary); font-size: 0.8rem; }

.stats {
  display: flex;
  justify-content: space-around;
  text-align: center;
  background-color: var(--color-background-soft);
  padding: 16px 8px;
  border-radius: 8px;
}
.stat-item span { color: var(--color-text-secondary); display: block; margin-bottom: 4px; font-size: 0.9rem; }
.stat-item strong { font-family: var(--font-display); font-size: 1.8rem; }

.card-footer {
  text-align: right;
  font-weight: 700;
  color: var(--color-accent-primary);
  opacity: 0;
  transition: opacity 0.3s ease;
}
.history-card:hover .card-footer, .history-card:focus .card-footer {
  opacity: 1;
}
</style>
