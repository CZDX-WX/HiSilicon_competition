import type { NormalizedLandmark } from '@mediapipe/tasks-vision';
import type { IExerciseAnalyzer, AnalysisResult, RepData } from './IExerciseAnalyzer';

class JumpingJackAnalyzer implements IExerciseAnalyzer {
  public readonly name = 'jumping_jack';

  private repCount = 0;
  private stage: 'closed' | 'open' = 'closed';
  private feedback = '';

  // --- 新增内部状态，用于追踪单次动作的数据 ---
  private maxAnkleSeparationRatioThisRep = 0; // 用于记录本次开合的最大幅度比例
  private lastRepTimestamp = 0;             // 上一次计数时的时间戳

  // --- 阈值定义 ---
  private readonly WRIST_ABOVE_SHOULDER_THRESHOLD = 0.05;
  private readonly ANKLE_SEPARATION_OPEN_RATIO = 1.0;
  private readonly ANKLE_SEPARATION_CLOSED_RATIO = 0.5;

  /**
   * 新增：在训练开始时调用，初始化计时起点
   */
  public start(): void {
    this.lastRepTimestamp = Date.now();
  }

  /**
   * 重置内部状态
   */
  public reset(): void {
    this.repCount = 0;
    this.stage = 'closed';
    this.feedback = '';
    this.maxAnkleSeparationRatioThisRep = 0;
    this.lastRepTimestamp = 0;
  }

  /**
   * 分析传入的姿态关键点
   */
  public analyze(landmarks: NormalizedLandmark[]): AnalysisResult {
    this.feedback = '';
    let newRepData: RepData | undefined = undefined;

    const LEFT_SHOULDER = 11, RIGHT_SHOULDER = 12;
    const LEFT_WRIST = 15, RIGHT_WRIST = 16;
    const LEFT_ANKLE = 27, RIGHT_ANKLE = 28;

    // 指标计算
    const leftArmUp = landmarks[LEFT_WRIST].y < landmarks[LEFT_SHOULDER].y - this.WRIST_ABOVE_SHOULDER_THRESHOLD;
    const rightArmUp = landmarks[RIGHT_WRIST].y < landmarks[RIGHT_SHOULDER].y - this.WRIST_ABOVE_SHOULDER_THRESHOLD;
    const armsAreUp = leftArmUp && rightArmUp;

    const shoulderWidth = Math.abs(landmarks[LEFT_SHOULDER].x - landmarks[RIGHT_SHOULDER].x);
    const ankleSeparation = Math.abs(landmarks[LEFT_ANKLE].x - landmarks[RIGHT_ANKLE].x);

    let legsAreOpen = false;
    let legsAreClosed = false;
    let currentSeparationRatio = 0;

    if (shoulderWidth > 0.01) {
      currentSeparationRatio = ankleSeparation / shoulderWidth;
      legsAreOpen = currentSeparationRatio > this.ANKLE_SEPARATION_OPEN_RATIO;
      legsAreClosed = currentSeparationRatio < this.ANKLE_SEPARATION_CLOSED_RATIO;
    }

    // 在打开阶段，持续更新本次的最大开合幅度
    if (this.stage === 'open') {
      this.maxAnkleSeparationRatioThisRep = Math.max(this.maxAnkleSeparationRatioThisRep, currentSeparationRatio);
    }

    // 状态机逻辑
    if (this.stage === 'closed' && armsAreUp && legsAreOpen) {
      this.stage = 'open';
      this.feedback = '动作到位！';
      // 进入打开阶段时，重置本次的最大幅度记录
      this.maxAnkleSeparationRatioThisRep = 0;
    }
    else if (this.stage === 'open' && !armsAreUp && legsAreClosed) {
      this.stage = 'closed';
      this.repCount++;
      this.feedback = '完成一次！';

      // --- 计算并打包本次动作的详细数据 ---
      const now = Date.now();
      const tempo = this.lastRepTimestamp === 0 ? 0 : (now - this.lastRepTimestamp) / 1000;
      this.lastRepTimestamp = now;

      newRepData = {
        timestamp: now,
        rangeOfMotion: parseFloat(this.maxAnkleSeparationRatioThisRep.toFixed(2)), // 幅度就是这次的最大开合比例
        tempo: parseFloat(tempo.toFixed(2)),
      };

      console.info(`%c[JumpingJack] 完成一次开合跳! 幅度比例: ${newRepData.rangeOfMotion}, 耗时: ${newRepData.tempo}s`, 'color: #007bff; font-weight: bold;');
    }

    // ... 质量反馈逻辑 ...

    return {
      repCount: this.repCount,
      feedback: this.feedback,
      stage: this.stage,
      newRepData,
    };
  }
}

export const jumpingJackAnalyzer = new JumpingJackAnalyzer();
