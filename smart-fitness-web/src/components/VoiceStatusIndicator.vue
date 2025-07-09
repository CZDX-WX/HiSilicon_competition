<script setup lang="ts">
import type { VoiceStatus } from '@/services/voiceControlService';
import { computed } from 'vue';

const props = defineProps<{ status: VoiceStatus }>();

const statusInfo = computed(() => {
  switch (props.status) {
    case 'listening':
      return { class: 'listening', title: '正在聆听...' };
    case 'stopped':
      return { class: 'stopped', title: '正在重启...' };
    case 'error':
      return { class: 'error', title: '识别出错' };
    default:
      return { class: 'inactive', title: '语音控制未激活' };
  }
});
</script>

<template>
  <div class="indicator-container" :class="statusInfo.class" :title="statusInfo.title">
    <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
      <path d="M12 14c1.66 0 3-1.34 3-3V5c0-1.66-1.34-3-3-3S9 3.34 9 5v6c0 1.66 1.34 3 3 3zm5.3-3c0 3-2.54 5.1-5.3 5.1S6.7 14 6.7 11H5c0 3.41 2.72 6.23 6 6.72V21h2v-3.28c3.28-.49 6-3.31 6-6.72h-1.7z"></path>
    </svg>
  </div>
</template>

<style scoped>
.indicator-container {
  /* position: fixed; bottom: 24px; right: 24px; */ /* <-- 移除或注释掉固定定位 */
  width: 40px; /* <-- 尺寸改小 */
  height: 40px; /* <-- 尺寸改小 */
  border-radius: 50%;
  display: flex;
  justify-content: center;
  align-items: center;
  background-color: transparent; /* 改为透明背景 */
  border: 2px solid grey;
  transition: all 0.3s ease;
  cursor: help; /* 增加一个问号光标，提示用户可以悬浮查看信息 */
}
.indicator-container svg {
  width: 20px; /* <-- 图标改小 */
  height: 20px; /* <-- 图标改小 */
  color: grey;
}

/* --- 状态样式 --- */
.indicator-container.listening {
  border-color: #3FB950;
  box-shadow: 0 0 15px #3FB950;
  animation: pulse 1.5s infinite;
}
.indicator-container.listening svg {
  color: #3FB950;
}

.indicator-container.stopped {
  border-color: #facc15;
}
.indicator-container.stopped svg {
  color: #facc15;
}

.indicator-container.error {
  border-color: #ef4444;
}
.indicator-container.error svg {
  color: #ef4444;
}

@keyframes pulse {
  0% { box-shadow: 0 0 0 0 rgba(63, 185, 80, 0.7); }
  70% { box-shadow: 0 0 10px 15px rgba(63, 185, 80, 0); }
  100% { box-shadow: 0 0 0 0 rgba(63, 185, 80, 0); }
}
</style>
