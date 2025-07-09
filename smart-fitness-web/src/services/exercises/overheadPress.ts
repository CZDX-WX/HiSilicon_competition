import type { NormalizedLandmark } from '@mediapipe/tasks-vision';
import type { IExerciseAnalyzer, AnalysisResult, RepData } from './IExerciseAnalyzer';

class OverheadPressAnalyzer implements IExerciseAnalyzer {
  public readonly name = 'overhead_press';

  private repCount = 0;
  private stage: 'down' | 'up' = 'down';
  private feedback = '';

  // --- 新增内部状态 ---
  private minWristYThisRep = 1.0; // y值越小越高，所以记录最小值
  private lastRepTimestamp = 0;
  // 新增 start 方法
  public start(): void {
    // 当训练开始时，将当前时间作为计时的起点
    this.lastRepTimestamp = Date.now();
  }
  public reset(): void {
    this.repCount = 0;
    this.stage = 'down';
    this.feedback = '';
    this.minWristYThisRep = 1.0;
    this.lastRepTimestamp = 0;
  }

  public analyze(landmarks: NormalizedLandmark[]): AnalysisResult {
    this.feedback = '';
    let newRepData: RepData | undefined = undefined;

    const LEFT_SHOULDER = 11, RIGHT_SHOULDER = 12;
    const LEFT_WRIST = 15, RIGHT_WRIST = 16;
    const NOSE = 0;

    const shoulderY = (landmarks[LEFT_SHOULDER].y + landmarks[RIGHT_SHOULDER].y) / 2;
    const wristY = (landmarks[LEFT_WRIST].y + landmarks[RIGHT_WRIST].y) / 2;
    const noseY = landmarks[NOSE].y;

    const armsAreUp = wristY < noseY;
    const armsAreDown = wristY > shoulderY;

    if (this.stage === 'up') {
      // 在举起阶段，持续更新本次的最高点 (最小y值)
      this.minWristYThisRep = Math.min(this.minWristYThisRep, wristY);
    }

    if (this.stage === 'down' && armsAreUp) {
      this.stage = 'up';
      // 进入举起阶段时，重置本次的最高点记录
      this.minWristYThisRep = 1.0;
    } else if (this.stage === 'up' && armsAreDown) {
      this.stage = 'down';
      this.repCount++;
      this.feedback = '完成一次！';

      const now = Date.now();
      const tempo = this.lastRepTimestamp === 0 ? 0 : (now - this.lastRepTimestamp) / 1000;
      this.lastRepTimestamp = now;

      newRepData = {
        timestamp: now,
        // 幅度可以用一个相对值表示，例如超过鼻子多少。这里我们直接记录y值，越小越好。
        rangeOfMotion: this.minWristYThisRep,
        tempo: parseFloat(tempo.toFixed(2)),
      };
      console.info(`%c[OverheadPress] 完成一次推举! 高度: ${newRepData.rangeOfMotion.toFixed(2)}, 耗时: ${newRepData.tempo}s`, 'color: #007bff; font-weight: bold;');
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

export const overheadPressAnalyzer = new OverheadPressAnalyzer();
