<script setup lang="ts">
// 这个组件的代码与之前“提供完整LocalCamera”时的版本完全相同
// 它只负责摄像头和AI分析，不涉及任何网络
import { ref, onMounted, onUnmounted } from 'vue';
import { useMediaPipe } from '@/composables/useMediaPipe';
import { useWorkoutStore } from '@/stores/workoutStore';
import { DrawingUtils, PoseLandmarker } from '@mediapipe/tasks-vision';

const { initializeMediaPipe, predictWebcam } = useMediaPipe();
const workoutStore = useWorkoutStore();

const videoEl = ref<HTMLVideoElement | null>(null);
const canvasEl = ref<HTMLCanvasElement | null>(null);
let rafId: number;
let ctx: CanvasRenderingContext2D | null = null;
let drawer: DrawingUtils | null = null;

const loop = () => {
  if (!videoEl.value || !canvasEl.value || !ctx || !drawer || videoEl.value.paused) {
    rafId = requestAnimationFrame(loop);
    return;
  }

  const video = videoEl.value;
  const canvas = canvasEl.value;

  if (canvas.width !== video.videoWidth) {
    canvas.width = video.videoWidth;
    canvas.height = video.videoHeight;
  }

  ctx.save();
  ctx.scale(-1, 1);
  ctx.drawImage(video, -canvas.width, 0, canvas.width, canvas.height);
  ctx.restore();

  const result = predictWebcam(video);
  const landmarks = result?.landmarks?.[0];
  workoutStore.processLandmarks(landmarks);

  if (workoutStore.systemStatus !== 'searching' && landmarks) {
    ctx.save();
    ctx.scale(-1, 1);
    ctx.translate(-canvas.width, 0);
    drawer.drawConnectors(landmarks, PoseLandmarker.POSE_CONNECTIONS, { color: '#58A6FF', lineWidth: 3 });
    drawer.drawLandmarks(landmarks, { radius: 4, color: '#3FB950', fillColor: '#E6EDF3' });
    ctx.restore();
  }

  rafId = requestAnimationFrame(loop);
};

const start = async () => {
  try {
    await initializeMediaPipe();
    workoutStore.initialize();
    ctx = canvasEl.value!.getContext('2d');
    drawer = new DrawingUtils(ctx!);
    loop();
  } catch (e) {
    console.error("AI引擎初始化失败", e);
    workoutStore.feedback = "AI引擎初始化失败";
  }
};

const setupCamera = async () => {
  if (!videoEl.value) return;
  try {
    const stream = await navigator.mediaDevices.getUserMedia({ video: { width: 1280, height: 720 } });
    videoEl.value.srcObject = stream;
    videoEl.value.onloadeddata = () => {
      videoEl.value?.play().then(start);
    };
  } catch (e) {
    console.error("摄像头访问失败", e);
    workoutStore.feedback = "摄像头访问失败";
  }
};

onMounted(setupCamera);

onUnmounted(() => {
  cancelAnimationFrame(rafId);
  const stream = videoEl.value?.srcObject as MediaStream | null;
  stream?.getTracks().forEach((t) => t.stop());
});
</script>

<template>
  <div class="camera-container">
    <video ref="videoEl" autoplay playsinline style="display: none;"></video>
    <canvas ref="canvasEl" class="output-canvas"></canvas>
  </div>
</template>

<style scoped>
.camera-container {
  position: relative; width: 100%; height: 100%;
  background: #000; border-radius: 12px; overflow: hidden;
}
.output-canvas {
  width: 100%; height: 100%; object-fit: contain;
}
</style>
