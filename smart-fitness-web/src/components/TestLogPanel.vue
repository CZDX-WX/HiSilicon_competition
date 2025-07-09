<script setup lang="ts">
import { ref, watch, nextTick,  onUnmounted } from 'vue';
import { useLogStore } from '@/stores/useLogStore';

const logStore = useLogStore();
const logContainer = ref<HTMLElement | null>(null);

const isVisible = ref(true); // 控制面板显示/隐藏
const panelPosition = ref({ x: 20, y: window.innerHeight - 370 }); // 初始位置
const isDragging = ref(false);
const dragStart = { x: 0, y: 0 };

// 自动滚动到底部
watch(logStore.logs, () => {
  nextTick(() => {
    if (logContainer.value) {
      logContainer.value.scrollTop = logContainer.value.scrollHeight;
    }
  });
}, { deep: true });

// --- 拖拽逻辑 ---
const onDragStart = (event: PointerEvent) => {
  isDragging.value = true;
  dragStart.x = event.clientX - panelPosition.value.x;
  dragStart.y = event.clientY - panelPosition.value.y;
  window.addEventListener('pointermove', onDragMove);
  window.addEventListener('pointerup', onDragEnd);
};

const onDragMove = (event: PointerEvent) => {
  if (isDragging.value) {
    panelPosition.value.x = event.clientX - dragStart.x;
    panelPosition.value.y = event.clientY - dragStart.y;
  }
};

const onDragEnd = () => {
  isDragging.value = false;
  window.removeEventListener('pointermove', onDragMove);
  window.removeEventListener('pointerup', onDragEnd);
};

onUnmounted(() => {
  window.removeEventListener('pointermove', onDragMove);
  window.removeEventListener('pointerup', onDragEnd);
});
</script>

<template>
  <Transition name="fade">
    <div
      v-if="isVisible"
      class="log-panel"
      :style="{ transform: `translate(${panelPosition.x}px, ${panelPosition.y}px)` }"
    >
      <div class="header" @pointerdown="onDragStart">
        <span>🧪 指令日志监视器</span>
        <button class="close-btn" @click="isVisible = false">×</button>
      </div>
      <div class="log-container" ref="logContainer">
        <div v-for="(log, index) in logStore.logs" :key="index" class="log-entry">
          <span class="timestamp">{{ log.timestamp }}</span>
          <span class="status-badge" :class="log.status">{{ log.status }}</span>
          <div class="command-info">
            <span class="key">{{ log.commandKey }}</span>
            <span class="arrow">&rarr;</span>
            <span class="value">{{ log.commandValue }}</span>
          </div>
          <div class="text-info">"{{ log.voiceText }}"</div>
        </div>
      </div>
    </div>
  </Transition>
</template>

<style scoped>
.log-panel {
  position: fixed;
  /* top和left由JS控制 */
  width: 480px;
  height: 350px;
  background-color: rgba(13, 17, 23, 0.9);
  backdrop-filter: blur(10px);
  border: 1px solid var(--color-border);
  border-radius: 8px;
  z-index: 2000;
  display: flex;
  flex-direction: column;
  font-family: 'Courier New', Courier, monospace;
  font-size: 13px;
  box-shadow: 0 5px 25px rgba(0, 0, 0, 0.5);
  color-scheme: dark;
}
.header {
  background-color: var(--color-background-soft);
  color: var(--color-text-secondary);
  padding: 8px 12px;
  font-weight: bold;
  border-bottom: 1px solid var(--color-border);
  cursor: move;
  display: flex;
  justify-content: space-between;
  align-items: center;
}
.close-btn {
  background: none;
  border: none;
  color: var(--color-text-secondary);
  font-size: 20px;
  font-weight: bold;
  cursor: pointer;
  padding: 0 8px;
}
.close-btn:hover {
  color: #fff;
}
.log-container {
  flex-grow: 1;
  overflow-y: auto;
  padding: 12px;
}
.log-entry {
  padding: 6px 4px;
  display: grid;
  grid-template-columns: 80px 1fr;
  align-items: center;
  gap: 4px 12px;
}
.log-entry:not(:last-child) {
  border-bottom: 1px dotted rgba(255, 255, 255, 0.1);
}
.timestamp {
  color: var(--color-text-secondary);
}
.status-badge {
  padding: 2px 6px;
  border-radius: 4px;
  font-size: 11px;
  font-weight: bold;
  text-align: center;
  justify-self: start;
}
.status-badge.ready { background-color: #2ea043; color: white; }
.status-badge.calibrating, .status-badge.waiting_confirmation { background-color: #dbab09; color: black; }
.status-badge.searching { background-color: #58a6ff; color: white; }

.command-info {
  grid-column: 2;
  display: flex;
  align-items: baseline;
  gap: 8px;
  font-weight: bold;
}
.key { color: #e3b341; }
.arrow { color: var(--color-text-secondary); }
.value { color: #58a6ff; font-size: 16px; }

.text-info {
  grid-column: 2;
  color: #a5d6ff;
  font-style: italic;
  font-size: 12px;
}

.fade-enter-active, .fade-leave-active { transition: opacity 0.3s ease; }
.fade-enter-from, .fade-leave-to { opacity: 0; }
</style>
