// 文件: src/composables/useMediaPipe.ts
import { ref, onUnmounted } from 'vue';
import { PoseLandmarker, FilesetResolver } from '@mediapipe/tasks-vision';
import type { PoseLandmarkerResult } from '@mediapipe/tasks-vision';

export const isMediaPipeInitialized = ref(false);
let poseLandmarker: PoseLandmarker | null = null;

export function useMediaPipe() {
  const initializeMediaPipe = async () => {
    try {
      const vision = await FilesetResolver.forVisionTasks('/wasm');
      poseLandmarker = await PoseLandmarker.createFromOptions(vision, {
        baseOptions: {
          modelAssetPath: `/models/pose_landmarker_lite.task`,
          delegate: 'GPU',
        },
        runningMode: 'VIDEO',
        numPoses: 1,
      });
      isMediaPipeInitialized.value = true;
      console.log('✅ MediaPipe PoseLandmarker 已从本地资源成功初始化！');
    } catch (error) {
      console.error('❌ 初始化MediaPipe失败!', error);
    }
  };

  const predictWebcam = (input: HTMLVideoElement | HTMLCanvasElement): PoseLandmarkerResult | null => {
    if (!poseLandmarker || !input) return null;

    // 如果是 video 元素，可以增加一个播放状态检查
    if (input instanceof HTMLVideoElement && (input.paused || input.ended || input.readyState < 3)) {
      return null;
    }

    return poseLandmarker.detectForVideo(input, performance.now());
  };

  onUnmounted(() => {
    poseLandmarker?.close();
    isMediaPipeInitialized.value = false;
  });

  return { initializeMediaPipe, predictWebcam };
}
