<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue';
import { useMediaPipe } from '@/composables/useMediaPipe';
import { useWorkoutStore } from '@/stores/workoutStore';
import { DrawingUtils, PoseLandmarker } from '@mediapipe/tasks-vision';

// --- 定义统一的、精细的加载状态类型 ---
type LoadingStatus = 'connecting_webrtc' | 'initializing_ai' | 'connected' | 'failed';

const { initializeMediaPipe, predictWebcam } = useMediaPipe();
const workoutStore = useWorkoutStore();

// --- DOM Refs ---
const videoRef = ref<HTMLVideoElement | null>(null);
const canvasRef = ref<HTMLCanvasElement | null>(null);

// --- 状态 Ref ---
const loadingStatus = ref<LoadingStatus>('connecting_webrtc');
const errorMessage = ref('');

// --- Computed ---
// 根据加载状态，动态显示不同的提示信息
const loadingMessage = computed(() => {
  switch (loadingStatus.value) {
    case 'connecting_webrtc':
      return '正在连接媒体服务器...';
    case 'initializing_ai':
      return 'AI引擎初始化中...';
    case 'failed':
      return `连接失败: ${errorMessage.value}`;
    case 'connected':
      return '连接成功';
    default:
      return '准备中...';
  }
});

// --- 内部变量 ---
let rafId: number;
let ctx: CanvasRenderingContext2D | null = null;
let drawer: DrawingUtils | null = null;
let pc: RTCPeerConnection | null = null;

/**
 * 主循环：取帧 -> 调用模型 -> 推送数据 -> 根据全局状态决定是否绘制
 */
const loop = () => {
  const video = videoRef.value;
  if (!video || !ctx || !drawer || video.paused || video.ended) {
    rafId = requestAnimationFrame(loop);
    return;
  }
  const result = predictWebcam(video);
  const landmarks = result?.landmarks?.[0];
  workoutStore.processLandmarks(landmarks);

  const w = video.videoWidth;
  const h = video.videoHeight;
  if (w && h && canvasRef.value) {
    canvasRef.value.width = w;
    canvasRef.value.height = h;
    ctx.clearRect(0, 0, w, h);
    ctx.drawImage(video, 0, 0, w, h);
  }

  if (workoutStore.systemStatus !== 'searching' && landmarks) {
    drawer.drawConnectors(landmarks, PoseLandmarker.POSE_CONNECTIONS, { color: '#58A6FF', lineWidth: 3 });
    drawer.drawLandmarks(landmarks, { radius: 4, color: '#3FB950', fillColor: '#E6EDF3' });
  }
  rafId = requestAnimationFrame(loop);
};

/**
 * 启动AI检测流程
 */
const startDetection = async () => {
  if (!videoRef.value || !canvasRef.value) return;
  loadingStatus.value = 'initializing_ai';
  try {
    await initializeMediaPipe();
    workoutStore.initialize();
    ctx = canvasRef.value.getContext('2d');
    drawer = new DrawingUtils(ctx!);
    loadingStatus.value = 'connected';
    loop();
  } catch (error) {
    console.error("AI引擎初始化失败:", error);
    errorMessage.value = 'AI引擎加载失败。';
    loadingStatus.value = 'failed';
  }
};

/**
 * WebRTC 连接逻辑
 */
const connectWebRTC = async () => {
  if (!videoRef.value) return;
  loadingStatus.value = 'connecting_webrtc';
  errorMessage.value = '';
  pc = new RTCPeerConnection();

  pc.ontrack = (event) => {
    if (event.track.kind === 'video' && videoRef.value) {
      videoRef.value.srcObject = event.streams[0];
    }
  };

  pc.onconnectionstatechange = () => {
    if (pc) {
      if (pc.connectionState === 'failed') {
        loadingStatus.value = 'failed';
        errorMessage.value = '媒体服务器连接中断。';
      } else if (pc.connectionState === 'closed' || pc.connectionState === 'disconnected') {
        if (loadingStatus.value !== 'connected') {
          loadingStatus.value = 'failed';
          errorMessage.value = '连接被关闭，请重试。';
        }
      }
    }
  };

  try {
    const offer = await pc.createOffer({ offerToReceiveVideo: true });
    await pc.setLocalDescription(offer);
    const response = await fetch('http://localhost:8080/offer', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ sdp: offer.sdp, type: offer.type }),
    });
    if (!response.ok) {
      throw new Error(`服务器响应错误: ${response.status} ${response.statusText}`);
    }
    const answer = await response.json();
    await pc.setRemoteDescription(answer);
  } catch (e) {
    console.error("WebRTC 连接失败:", e);
    errorMessage.value = '无法连接服务器，请确认后端服务已运行。';
    loadingStatus.value = 'failed';
  }
};

/**
 * 重连函数
 */
const retryConnection = () => {
  console.log("尝试重新连接...");
  if (pc) {
    pc.close();
  }
  connectWebRTC();
};

onMounted(() => {
  videoRef.value?.addEventListener('playing', startDetection);
  connectWebRTC();
});

onUnmounted(() => {
  cancelAnimationFrame(rafId);
  pc?.close();
  const stream = videoRef.value?.srcObject as MediaStream | null;
  stream?.getTracks().forEach((t) => t.stop());
});
</script>

<template>
  <div class="camera-container">
    <Transition name="fade">
      <div v-if="loadingStatus !== 'connected'" class="loading-overlay">
        <div class="loader-content">
          <div v-if="loadingStatus === 'connecting_webrtc' || loadingStatus === 'initializing_ai'" class="spinner"></div>
          <div class="loader-text">{{ loadingMessage }}</div>
          <button v-if="loadingStatus === 'failed'" @click="retryConnection" class="retry-btn">
            重新连接
          </button>
        </div>
      </div>
    </Transition>

    <video ref="videoRef" autoplay playsinline class="output-canvas"></video>
    <canvas ref="canvasRef" class="output-canvas"></canvas>
  </div>
</template>

<style scoped>
.camera-container {
  position: relative;
  width: 100%;
  height: 100%;
  background: #000;
  border-radius: 12px;
  overflow: hidden;
}
.output-canvas, video {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  transform: scaleX(-1);

  /* --- 核心修复：添加这一行神奇的CSS --- */
  object-fit: contain; /* 保持视频自身宽高比，用黑边填充容器空白区域 */
}
canvas {
  z-index: 5;
  background-color: transparent;
}
video {
  z-index: 1;
}
.loading-overlay {
  position: absolute;
  inset: 0;
  background: rgba(13, 17, 23, 0.9);
  display: flex;
  justify-content: center;
  align-items: center;
  z-index: 10;
}
.loader-content {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 20px;
}
.loader-text {
  font-family: var(--font-display);
  color: var(--color-text-primary);
  font-size: 1.5rem;
  text-align: center;
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
.retry-btn {
  font-family: var(--font-primary);
  font-weight: 700;
  font-size: 1rem;
  color: #fff;
  background-color: var(--color-accent-primary);
  border: none;
  border-radius: 6px;
  padding: 10px 20px;
  margin-top: 10px;
  cursor: pointer;
  transition: all 0.2s ease;
}
.retry-btn:hover {
  filter: brightness(1.2);
}
.fade-enter-active,
.fade-leave-active {
  transition: opacity 0.5s ease;
}
.fade-enter-from,
.fade-leave-to {
  opacity: 0;
}
</style>
