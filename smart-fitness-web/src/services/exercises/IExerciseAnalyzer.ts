import type { NormalizedLandmark } from '@mediapipe/tasks-vision';

/**
 * 新增：定义单次动作的详细数据结构
 */
export interface RepData {
  /** 完成该次动作的时间戳 */
  timestamp: number;
  /** 动作幅度 (例如，角度或距离) */
  rangeOfMotion: number;
  /** 动作耗时，单位：秒 */
  tempo: number;
}

/**
 * 更新：分析结果接口现在可以附带单次动作的详细数据
 */
export interface AnalysisResult {
  repCount: number;
  feedback: string;
  stage: string;
  /** (可选) 当且仅当完成一次新动作时，附带该次动作的详细数据 */
  newRepData?: RepData;
}

export interface IExerciseAnalyzer {
  readonly name: string;
  /** 新增：在训练开始时调用，用于初始化计时器等 */
  start(): void;
  reset(): void;
  analyze(landmarks: NormalizedLandmark[]): AnalysisResult;
}
