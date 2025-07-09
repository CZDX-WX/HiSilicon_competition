import { calculateAngle } from '@/utils/vectorUtils';
import type { NormalizedLandmark } from '@mediapipe/tasks-vision';
import type { IExerciseAnalyzer, AnalysisResult, RepData } from './IExerciseAnalyzer';

class LateralRaiseAnalyzer implements IExerciseAnalyzer {
  public readonly name = 'lateral_raise';

  private repCount = 0;
  private stage: 'down' | 'up' = 'down';
  private feedback = '';

  // --- 新增内部状态 ---
  private maxArmAngleThisRep = 0;
  private lastRepTimestamp = 0;

  private readonly ARM_UP_THRESHOLD = 75;
  private readonly ARM_DOWN_THRESHOLD = 30;
  // 新增 start 方法
  public start(): void {
    // 当训练开始时，将当前时间作为计时的起点
    this.lastRepTimestamp = Date.now();
  }
  public reset(): void {
    this.repCount = 0;
    this.stage = 'down';
    this.feedback = '';
    this.maxArmAngleThisRep = 0;
    this.lastRepTimestamp = 0;
  }

  public analyze(landmarks: NormalizedLandmark[]): AnalysisResult {
    this.feedback = '';
    let newRepData: RepData | undefined = undefined;

    const LEFT_SHOULDER = 11, RIGHT_SHOULDER = 12;
    const LEFT_HIP = 23, RIGHT_HIP = 24;
    const LEFT_ELBOW = 13, RIGHT_ELBOW = 14;

    const leftArmAngle = calculateAngle(landmarks[LEFT_HIP], landmarks[LEFT_SHOULDER], landmarks[LEFT_ELBOW]);
    const rightArmAngle = calculateAngle(landmarks[RIGHT_HIP], landmarks[RIGHT_SHOULDER], landmarks[RIGHT_ELBOW]);

    if (this.stage === 'up') {
      // 在抬起阶段，持续更新本次的最大手臂角度
      this.maxArmAngleThisRep = Math.max(this.maxArmAngleThisRep, leftArmAngle, rightArmAngle);
    }

    if (this.stage === 'down' && leftArmAngle > this.ARM_UP_THRESHOLD && rightArmAngle > this.ARM_UP_THRESHOLD) {
      this.stage = 'up';
      // 进入抬起阶段时，重置本次的最大角度记录
      this.maxArmAngleThisRep = 0;
    } else if (this.stage === 'up' && leftArmAngle < this.ARM_DOWN_THRESHOLD && rightArmAngle < this.ARM_DOWN_THRESHOLD) {
      this.stage = 'down';
      this.repCount++;
      this.feedback = '完成一次！';

      const now = Date.now();
      const tempo = this.lastRepTimestamp === 0 ? 0 : (now - this.lastRepTimestamp) / 1000;
      this.lastRepTimestamp = now;

      newRepData = {
        timestamp: now,
        rangeOfMotion: this.maxArmAngleThisRep,
        tempo: parseFloat(tempo.toFixed(2)),
      };
      console.info(`%c[LateralRaise] 完成一次侧平举! 幅度: ${newRepData.rangeOfMotion.toFixed(1)}°, 耗时: ${newRepData.tempo}s`, 'color: #007bff; font-weight: bold;');
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

export const lateralRaiseAnalyzer = new LateralRaiseAnalyzer();
