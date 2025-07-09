<script setup lang="ts">
import { ref,onMounted } from 'vue';
import { RouterLink, RouterView } from 'vue-router';
import WorkoutSummaryModal from '@/components/WorkoutSummaryModal.vue';
import SettingsModal from '@/components/SettingsModal.vue';
import VoiceStatusIndicator from '@/components/VoiceStatusIndicator.vue';
import { useWorkoutStore } from '@/stores/workoutStore';
import TestLogPanel from '@/components/TestLogPanel.vue'

const isSettingsModalOpen = ref(false);
const workoutStore = useWorkoutStore();

import { webSocketService } from '@/services/websocketService';
onMounted(() => {
  const wsUrl = `ws://${window.location.hostname}:8080`;
  console.log(`[INIT] 正在连接指令通道: ${wsUrl}`);
  const ws = new WebSocket(wsUrl);

  // 将连接实例共享给全局服务，以便 workoutStore 调用
  webSocketService.ws.value = ws;

  ws.onopen = () => console.log("✅ 指令通道已连接。");
  ws.onclose = () => console.log("❌ 指令通道已断开。");
  ws.onerror = (e) => console.error("指令通道出错:", e);
});

</script>

<template>
  <div class="app-container">
    <header class="app-header">
      <div class="logo">
        <img alt="App logo" class="logo-img" src="@/assets/logo.svg" />
        <span>智能健身镜</span>
      </div>
      <div class="nav-and-settings">
        <nav class="navigation">
          <RouterLink to="/">实时训练</RouterLink>
          <RouterLink to="/history">历史记录</RouterLink>
        </nav>
        <VoiceStatusIndicator :status="workoutStore.voiceStatus" />
        <button @click="isSettingsModalOpen = true" class="settings-btn" title="个人设置">
          ⚙️
        </button>
      </div>
    </header>

    <main class="app-main-content">
      <RouterView />
    </main>
    <TestLogPanel />
    <WorkoutSummaryModal />

    <SettingsModal v-model="isSettingsModalOpen" />
  </div>
</template>

<style scoped>
/* 样式无需修改 */
.app-container {
  display: flex;
  flex-direction: column;
  height: 100vh;
  background-color: var(--color-background);
}

.app-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0 24px;
  height: 60px;
  background-color: var(--color-background-soft);
  border-bottom: 1px solid var(--color-border);
  flex-shrink: 0;
  z-index: 10;
}

.logo {
  display: flex;
  align-items: center;
  gap: 12px;
  font-family: var(--font-display);
  font-size: 1.2rem;
  font-weight: bold;
  color: var(--color-text-primary);
}

.logo-img {
  height: 32px;
  width: 32px;
}

.nav-and-settings {
  display: flex;
  align-items: center;
  gap: 16px;
}

.navigation a {
  font-weight: 700;
  color: var(--color-text-secondary);
  text-decoration: none;
  padding: 8px 16px;
  border-radius: 6px;
  transition: all 0.2s ease-in-out;
}

.navigation a:hover {
  background-color: rgba(88, 166, 255, 0.1);
  color: var(--color-accent-primary);
}

.navigation a.router-link-exact-active {
  color: var(--color-accent-primary);
  box-shadow: 0 0 15px rgba(88, 166, 255, 0.3), inset 0 0 5px rgba(88, 166, 255, 0.2);
}

.settings-btn {
  background: none;
  border: none;
  color: var(--color-text-secondary);
  font-size: 1.5rem;
  padding: 0;
  width: 40px;
  height: 40px;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  cursor: pointer;
  transition: all 0.2s ease;
}

.settings-btn:hover {
  background-color: rgba(255, 255, 255, 0.1);
  color: var(--color-text-primary);
}

.app-main-content {
  flex-grow: 1;
  overflow-y: auto;
}
</style>
