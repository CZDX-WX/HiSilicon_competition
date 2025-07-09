<script setup lang="ts">
import { computed } from 'vue';
import { useWorkoutStore } from '@/stores/workoutStore';

const workoutStore = useWorkoutStore();

const timerMinutes = computed(() => {
  return Math.floor(workoutStore.timer / 60).toString().padStart(2, '0');
});

const timerSeconds = computed(() => {
  return (workoutStore.timer % 60).toString().padStart(2, '0');
});

const timerForDatetime = computed(() => {
  const minutes = Math.floor(workoutStore.timer / 60);
  const seconds = workoutStore.timer % 60;
  return `PT${minutes}M${seconds}S`;
});
</script>

<template>
  <div class="dashboard">
    <div v-if="workoutStore.systemStatus !== 'ready' && workoutStore.systemStatus !== 'showing_report'" class="system-status-overlay glass-card">
      <div v-if="workoutStore.systemStatus === 'initializing' || workoutStore.systemStatus === 'searching'" class="spinner"></div>
      <span class="feedback-text">{{ workoutStore.feedback }}</span>

      <button
        v-if="workoutStore.systemStatus === 'waiting_confirmation'"
        @click="workoutStore.confirmCalibration"
        class="confirm-btn">
        确认姿态
      </button>
    </div>

    <div v-else class="data-container">
      <div class="data-grid">
        <div class="data-card glass-card">
          <span class="label">动作计数</span>
          <span class="value">{{ workoutStore.repCount }}</span>
        </div>

        <div class="data-card glass-card">
          <span class="label">运动计时</span>
          <time :datetime="timerForDatetime" class="timer-display">
            <div class="time-part">
              <span class="time-value">{{ timerMinutes }}</span>
              <span class="time-label">MIN</span>
            </div>
            <span class="time-separator">:</span>
            <div class="time-part">
              <span class="time-value">{{ timerSeconds }}</span>
              <span class="time-label">SEC</span>
            </div>
          </time>
        </div>

        <div class="data-card glass-card" style="grid-column: 1 / -1;">
          <span class="label">热量消耗 (千卡)</span>
          <span class="value">{{ workoutStore.calories.toFixed(1) }}</span>
        </div>
      </div>

      <div class="feedback-card glass-card">
        <span class="label">实时反馈</span>
        <div class="feedback-content">
          <span class="feedback-text">"{{ workoutStore.feedback }}"</span>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.glass-card {
  background: rgba(22, 27, 34, 0.5);
  backdrop-filter: blur(10px);
  border: 1px solid var(--color-border);
  border-radius: 12px;
  transition: all 0.3s ease;
}
.glass-card:hover {
  border-color: rgba(88, 166, 255, 0.5);
  box-shadow: 0 0 20px rgba(88, 166, 255, 0.2);
}
.dashboard {
  display: flex;
  flex-direction: column;
  height: 100%;
}
.system-status-overlay {
  height: 100%;
  display: flex;
  flex-direction: column;
  justify-content: center;
  align-items: center;
  padding: 20px;
  gap: 20px;
}
.data-container {
  display: flex;
  flex-direction: column;
  gap: 20px;
  height: 100%;
}
.data-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 20px;
}
.data-card {
  padding: 20px;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
}
.label {
  font-size: 1rem;
  color: var(--color-text-secondary);
  margin-bottom: 8px;
}
.value {
  font-family: var(--font-display);
  font-size: 2.5rem;
  font-weight: bold;
  color: var(--color-text-primary);
}
.timer-display {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 10px;
}
.time-part {
  display: flex;
  flex-direction: column;
  align-items: center;
}
.time-value {
  font-family: var(--font-display);
  font-size: 2.5rem;
  font-weight: bold;
  color: var(--color-text-primary);
  line-height: 1;
}
.time-label {
  font-size: 0.75rem;
  color: var(--color-text-secondary);
  letter-spacing: 1px;
}
.time-separator {
  font-family: var(--font-display);
  font-size: 2rem;
  color: var(--color-accent-primary);
  transform: translateY(-4px);
}
.feedback-card {
  flex-grow: 1;
  display: flex;
  flex-direction: column;
  min-height: 0;
  padding: 20px;
}
.feedback-content {
  flex-grow: 1;
  display: flex;
  flex-direction: column;
  justify-content: center;
  align-items: center;
  overflow-y: auto;
}
.feedback-text {
  font-size: 1.5rem;
  font-style: italic;
  color: var(--color-accent-primary);
  text-align: center;
}
.confirm-btn {
  font-family: var(--font-primary);
  font-weight: 700;
  font-size: 1.2rem;
  color: #fff;
  background-image: linear-gradient(to right, var(--color-accent-primary) 0%, #3FB950 100%);
  border: none;
  border-radius: 8px;
  padding: 12px 24px;
  margin-top: 20px;
  cursor: pointer;
  transition: all 0.2s ease;
  box-shadow: 0 4px 15px rgba(0, 0, 0, 0.2);
}
.confirm-btn:hover {
  transform: translateY(-2px);
  box-shadow: 0 6px 20px rgba(88, 166, 255, 0.4);
}
.spinner {
  width: 50px;
  height: 50px;
  border: 5px solid var(--color-border);
  border-top-color: var(--color-accent-primary);
  border-radius: 50%;
  animation: spin 1s linear infinite;
}
@keyframes spin {
  to {
    transform: rotate(360deg);
  }
}
</style>
