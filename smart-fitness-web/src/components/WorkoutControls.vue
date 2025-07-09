<script setup lang="ts">
import { useWorkoutStore } from '@/stores/workoutStore';
import { type WorkoutType } from '@/services/exerciseService' ;
const workoutStore = useWorkoutStore();

const handleSelectChange = (event: Event) => {
  const target = event.target as HTMLSelectElement;
  workoutStore.selectWorkout(target.value as WorkoutType | null);
};

// 用于美化下拉菜单显示的文本
const workoutNameMap: Record<WorkoutType, string> = {
  squat: '标准深蹲',
  jumping_jack: '开合跳',
  bicep_curl: '二头弯举',
  lateral_raise:'侧平举',
  overhead_press:'过头推举',
  lunge: '弓步',
  front_kick: '前踢腿'
};
</script>

<template>
  <div class="controls-container glass-card">
    <div class="control-group">
      <label for="workout-select">运动模式</label>
      <div class="select-wrapper">
        <select
          id="workout-select"
          @change="handleSelectChange"
          :disabled="workoutStore.systemStatus !== 'ready' || workoutStore.exerciseState !== 'idle'"
          :value="workoutStore.currentWorkout ?? ''"
        >
          <option disabled value="">--请选择一项运动--</option>
          <option
            v-for="workout in workoutStore.availableWorkouts"
            :key="workout"
            :value="workout"
          >
            {{ workoutNameMap[workout] || workout }}
          </option>
        </select>
      </div>
    </div>

    <div class="control-group">
      <label>训练控制</label>
      <div class="button-group">
        <button
          v-if="workoutStore.exerciseState === 'idle' || workoutStore.exerciseState === 'paused'"
          @click="workoutStore.startWorkout"
          class="btn btn-start"
          :disabled="workoutStore.systemStatus !== 'ready' || !workoutStore.currentWorkout"
        >
          {{ workoutStore.exerciseState === 'idle' ? '开始训练' : '继续' }}
        </button>
        <button v-if="workoutStore.exerciseState === 'running'" @click="workoutStore.pauseWorkout" class="btn btn-pause">
          暂停
        </button>
        <button
          v-if="workoutStore.exerciseState !== 'idle'"
          @click="workoutStore.stopWorkout"
          class="btn btn-reset"
        >
          结束
        </button>
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

.controls-container {
  padding: 20px;
  display: flex;
  flex-direction: column;
  gap: 24px;
}

.control-group {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.control-group label {
  font-weight: 700;
  color: var(--color-text-secondary);
  text-transform: uppercase;
  font-size: 0.8rem;
  letter-spacing: 1px;
}

.select-wrapper {
  position: relative;
}

.select-wrapper::after {
  content: '▼';
  font-size: 12px;
  color: var(--color-accent-primary);
  position: absolute;
  right: 15px;
  top: 50%;
  transform: translateY(-50%);
  pointer-events: none;
}

select {
  width: 100%;
  padding: 12px 40px 12px 15px;
  border-radius: 6px;
  border: 1px solid var(--color-border);
  background-color: var(--color-background-soft);
  color: var(--color-text-primary);
  font-size: 1rem;
  font-family: var(--font-primary);
  -webkit-appearance: none;
  -moz-appearance: none;
  appearance: none;
  cursor: pointer;
  transition: all 0.2s ease;
}

select:hover {
  border-color: var(--color-accent-primary);
}

select:disabled {
  opacity: 0.5;
  cursor: not-allowed;
  border-color: var(--color-border);
}

.button-group {
  display: flex;
  gap: 12px;
}

.btn {
  flex-grow: 1;
  padding: 12px;
  border: 1px solid transparent;
  border-radius: 6px;
  font-size: 1rem;
  font-family: var(--font-display);
  font-weight: 700;
  cursor: pointer;
  transition: all 0.2s ease-in-out;
  position: relative;
  overflow: hidden;
  background-size: 200% auto;
}
.btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
  box-shadow: none;
  background-image: none;
  background-color: var(--color-background-soft);
  border-color: var(--color-border);
  color: var(--color-text-secondary);
}

.btn::before {
  content: '';
  position: absolute;
  top: -50%;
  left: -50%;
  width: 200%;
  height: 200%;
  background: radial-gradient(circle, rgba(255, 255, 255, 0.2), transparent 40%);
  transition: opacity 0.5s;
  opacity: 0;
}

.btn:hover:not(:disabled)::before {
  opacity: 1;
}

.btn-start {
  background-image: linear-gradient(to right, var(--color-accent-primary) 0%, #3FB950 100%);
  color: #fff;
  border-color: var(--color-accent-primary);
}

.btn-start:hover:not(:disabled) {
  box-shadow: 0 0 20px rgba(88, 166, 255, 0.5);
}

.btn-pause {
  background-color: #D29922;
  color: #fff;
  border-color: #D29922;
}
.btn-pause:hover:not(:disabled) {
  box-shadow: 0 0 20px rgba(210, 153, 34, 0.5);
}

.btn-reset {
  background-color: transparent;
  color: var(--color-text-secondary);
  border-color: var(--color-border);
}

.btn-reset:hover:not(:disabled) {
  border-color: var(--color-text-secondary);
  background-color: var(--color-background-soft);
  color: #fff;
}
</style>
