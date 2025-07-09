import { calculateAngle } from '@/utils/vectorUtils';
import type { NormalizedLandmark } from '@mediapipe/tasks-vision';
import type { IExerciseAnalyzer, AnalysisResult, RepData } from './IExerciseAnalyzer';

class FrontKickAnalyzer implements IExerciseAnalyzer {
  public readonly name = 'front_kick';

  private repCount = 0;
  private stage: 'down' | 'kick' = 'down';
  private feedback = '';
  private lastRepTimestamp = 0;
  private maxHipAngleThisRep = 0; // 记录踢腿最大高度（髋角越小越高）

  private readonly HIP_KICK_THRESHOLD = 140; // 髋关节角度小于140度视为踢腿
  private readonly HIP_DOWN_THRESHOLD = 170; // 髋关节角度大于170度视为放下

  public start(): void { this.lastRepTimestamp = Date.now(); }

  public reset(): void {
    this.repCount = 0;
    this.stage = 'down';
    this.feedback = '';
    this.lastRepTimestamp = 0;
    this.maxHipAngleThisRep = 0;
  }

  public analyze(landmarks: NormalizedLandmark[]): AnalysisResult {
    this.feedback = '';
    let newRepData: RepData | undefined = undefined;

    const [L_S, R_S, L_H, R_H, L_K, R_K] = [11, 12, 23, 24, 25, 26];

    // 我们以右腿为例进行检测
    const rightHipAngle = calculateAngle(landmarks[R_S], landmarks[R_H], landmarks[R_K]);

    if (this.stage === 'kick') {
      // 在踢腿阶段，记录踢到的最高点（最小角度）
      // 为了方便图表展示（越大越好），我们用 180 - 角度
      const kickHeight = 180 - rightHipAngle;
      this.maxHipAngleThisRep = Math.max(this.maxHipAngleThisRep, kickHeight);
    }

    if (this.stage === 'down' && rightHipAngle < this.HIP_KICK_THRESHOLD) {
      this.stage = 'kick';
      this.maxHipAngleThisRep = 0; // 重置
    } else if (this.stage === 'kick' && rightHipAngle > this.HIP_DOWN_THRESHOLD) {
      this.stage = 'down';
      this.repCount++;
      this.feedback = '完成一次！';

      const now = Date.now();
      const tempo = this.lastRepTimestamp === 0 ? 0 : (now - this.lastRepTimestamp) / 1000;
      this.lastRepTimestamp = now;

      newRepData = {
        timestamp: now,
        rangeOfMotion: parseFloat(this.maxHipAngleThisRep.toFixed(1)),
        tempo: parseFloat(tempo.toFixed(2)),
      };
    }

    if (this.stage === 'kick' && rightHipAngle > 150) {
      this.feedback = '踢得再高一点！';
    }

    return { repCount: this.repCount, feedback: this.feedback, stage: this.stage, newRepData };
  }
}

export const frontKickAnalyzer = new FrontKickAnalyzer();
