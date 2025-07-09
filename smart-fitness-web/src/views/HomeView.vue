<script setup lang="ts">
import CameraCanvas from '@/components/CameraCanvas.vue';
import DataDashboard from '@/components/DataDashboard.vue';
import WorkoutControls from '@/components/WorkoutControls.vue';
import WebSocketCanvas from '@/components/WebSocketCanvas.vue'
import LocalCamera from '@/components/LocalCamera.vue'
</script>

<template>
  <main class="main-layout">
    <div class="video-section">
<!--      <CameraCanvas />-->
      <WebSocketCanvas />
<!--      <LocalCamera />-->
    </div>

    <div class="dashboard-section">
      <WorkoutControls />
      <DataDashboard />
    </div>
  </main>
</template>

<style scoped>
.main-layout {
  display: flex;
  gap: 24px;
  padding: 24px;
  height: 100%;
  box-sizing: border-box;
}

.video-section {
  flex: 3;
  min-width: 0;
  display: flex;
}

.dashboard-section {
  flex: 2;
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 24px;
}
</style>
