<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue';
import { useMediaPipe, isMediaPipeInitialized } from '@/composables/useMediaPipe';
import { useWorkoutStore } from '@/stores/workoutStore';
import { DrawingUtils, PoseLandmarker } from '@mediapipe/tasks-vision';
import { webSocketService } from '@/services/websocketService';

// 定义组件自身的加载/连接状态
type ComponentStatus = 'initializing_ai' | 'connecting_ws' | 'connected' | 'failed';

const { initializeMediaPipe, predictWebcam } = useMediaPipe();
const workoutStore = useWorkoutStore();

// --- Refs ---
const canvasRef = ref<HTMLCanvasElement | null>(null);
const status = ref<ComponentStatus>('initializing_ai');
const errorMessage = ref('');

// --- 内部变量 ---
let ctx: CanvasRenderingContext2D | null = null;
let drawer: DrawingUtils | null = null;
let ws: WebSocket | null = null;

// --- Computed ---
const loadingMessage = computed(() => {
  switch (status.value) {
    case 'initializing_ai': return '1/2: 正在初始化AI引擎...';
    case 'connecting_ws': return '2/2: 正在连接媒体服务器...';
    case 'failed': return `出现错误: ${errorMessage.value}`;
    default: return '准备就绪';
  }
});
const showOverlay = computed(() => status.value !== 'connected');

/**
 * 启动流程的总控制函数
 */
const start = async () => {
  try {
    // 步骤1：初始化AI引擎
    status.value = 'initializing_ai';
    await initializeMediaPipe();

    // 获取2D上下文和绘制工具
    ctx = canvasRef.value!.getContext('2d');
    drawer = new DrawingUtils(ctx!);

    // 步骤2：AI就绪后，连接WebSocket
    setupWebSocket();

  } catch (error) {
    console.error("启动流程失败:", error);
    status.value = 'failed';
    errorMessage.value = 'AI引擎初始化失败。';
  }
};

/**
 * 初始化 WebSocket 连接，并定义核心处理逻辑
 */
const setupWebSocket = () => {
  status.value = 'connecting_ws';
  const wsUrl = `ws://${window.location.hostname}:8080`;
  ws = new WebSocket(wsUrl);

  // 将创建的实例共享给全局服务
  webSocketService.ws.value = ws;
  ws.binaryType = 'blob';

  ws.onopen = () => {
    console.log('✅ WebSocket 连接成功! 启动应用状态机...');
    status.value = 'connected';
    webSocketService.sendCommand( { command: 1} );
    // 只有在WebSocket连接成功后，才启动应用的状态机！
    workoutStore.initialize();
  };

  /**
   * 核心逻辑：应用的驱动核心，由新数据帧的到达来触发
   */
  ws.onmessage = async (event) => {
    if (!(event.data instanceof Blob) || !ctx || !drawer || !canvasRef.value) {
      return;
    }

    try {
      const frameBitmap = await createImageBitmap(event.data);
      const canvas = canvasRef.value;

      if (canvas.width !== frameBitmap.width || canvas.height !== frameBitmap.height) {
        canvas.width = frameBitmap.width;
        canvas.height = frameBitmap.height;
      }

      // 步骤A：在原始(非镜像)图像上进行分析
      ctx.drawImage(frameBitmap, 0, 0);
      const result = predictWebcam(canvas);
      const landmarks = result?.landmarks?.[0];
      workoutStore.processLandmarks(landmarks);

      // 步骤B：绘制用户可见的镜像画面
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      ctx.save();
      ctx.scale(-1, 1);
      ctx.translate(-canvas.width, 0);
      ctx.drawImage(frameBitmap, 0, 0);

      // 根据 workoutStore 的状态决定是否绘制骨骼
      if (workoutStore.systemStatus !== 'searching' && landmarks) {
        drawer.drawConnectors(landmarks, PoseLandmarker.POSE_CONNECTIONS, { color: '#58A6FF', lineWidth: 3 });
        drawer.drawLandmarks(landmarks, { radius: 4, color: '#3FB950', fillColor: '#E6EDF3' });
      }

      ctx.restore();
      frameBitmap.close();
    } catch (error) {
      console.error("处理WebSocket帧时出错:", error)
    }
  };

  ws.onclose = () => {
    console.error('❌ WebSocket 连接已关闭。');
    // 只有在未成功连接时才标记为失败，避免已连接后关闭也显示错误
    if(status.value !== 'connected') {
      status.value = 'failed';
      errorMessage.value = '连接被关闭，请检查后端服务。';
    } else {
      // 如果是已连接后断开，只在后台提示
      workoutStore.feedback = "与服务器的连接已断开。";
    }
  };

  ws.onerror = () => {
    console.error('WebSocket 发生错误');
    status.value = 'failed';
    errorMessage.value = '无法连接WebSocket服务器。';
  };
};

onMounted(start);

onUnmounted(() => {
  ws?.close();
});
</script>

<template>
  <div class="camera-container">
    <Transition name="fade">
      <div v-if="showOverlay" class="loading-overlay">
        <div class="loader-content">
          <div v-if="status !== 'failed'" class="spinner"></div>
          <div class="loader-text">{{ loadingMessage }}</div>
        </div>
      </div>
    </Transition>

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
.output-canvas {
  width: 100%;
  height: 100%;
  object-fit: cover;
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
.fade-enter-active,
.fade-leave-active {
  transition: opacity 0.5s ease;
}
.fade-enter-from,
.fade-leave-to {
  opacity: 0;
}
</style>
