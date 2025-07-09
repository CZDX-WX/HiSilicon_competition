import type { NormalizedLandmark } from '@mediapipe/tasks-vision';

/**
 * 计算由三个点构成的角度
 * @param a - 第一个点 (例如：肩膀)
 * @param b - 第二个点，作为角度的顶点 (例如：手肘)
 * @param c - 第三个点 (例如：手腕)
 * @returns 返回 0-180 之间的角度值
 */
export function calculateAngle(a: NormalizedLandmark, b: NormalizedLandmark, c: NormalizedLandmark): number {
  // 计算向量 ba 和 bc
  const ba_x = a.x - b.x;
  const ba_y = a.y - b.y;
  const bc_x = c.x - b.x;
  const bc_y = c.y - b.y;

  // 计算点积
  const dotProduct = ba_x * bc_x + ba_y * bc_y;

  // 计算向量模长
  const magnitudeA = Math.sqrt(ba_x * ba_x + ba_y * ba_y);
  const magnitudeB = Math.sqrt(bc_x * bc_x + bc_y * bc_y);

  // 计算夹角（弧度）
  const angleRad = Math.acos(dotProduct / (magnitudeA * magnitudeB));

  // 将弧度转换为角度
  return angleRad * 180 / Math.PI;
}
