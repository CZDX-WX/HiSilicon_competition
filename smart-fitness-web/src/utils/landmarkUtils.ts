import type { NormalizedLandmark } from '@mediapipe/tasks-vision';
import { calculateAngle } from './vectorUtils';

/**
 * 【最终版守门员】
 * 仅在系统开始前使用，用于检查一个姿态是否为有效的、可用于“解锁”应用的初始站立姿态。
 * @param landmarks - MediaPipe 返回的单人关键点数组
 * @returns 如果姿态有效则返回 true
 */
export function isInitialPoseValid(
  landmarks: NormalizedLandmark[] | undefined
): landmarks is NormalizedLandmark[] {
  // 关卡1：数据必须存在且不为空
  if (!landmarks || landmarks.length === 0) {
    return false;
  }

  // 关卡2：核心关键点的置信度必须达标
  const visibilityThreshold = 0.6;
  const keyLandmarks = [landmarks[11], landmarks[12], landmarks[23], landmarks[24]]; // 肩膀和臀部
  const validPointsCount = keyLandmarks.filter(
    p => p && (p.visibility != null && p.visibility > visibilityThreshold)
  ).length;

  if (validPointsCount < 3) {
    return false;
  }

  // 关卡3：垂直延展度必须达标
  const spreadThreshold = 0.5;
  let minY = 1.0;
  let maxY = 0.0;
  for (const landmark of landmarks) {
    if (landmark.y < minY) minY = landmark.y;
    if (landmark.y > maxY) maxY = landmark.y;
  }
  const spread = maxY - minY;
  if (spread < spreadThreshold) {
    return false;
  }

  return true;
}

// 导出 calculateAngle 供运动分析器使用
export { calculateAngle } from './vectorUtils';
