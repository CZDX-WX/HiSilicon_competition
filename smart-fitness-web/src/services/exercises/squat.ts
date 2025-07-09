import { calculateAngle } from '@/utils/vectorUtils';
import type { NormalizedLandmark } from '@mediapipe/tasks-vision';
import type { IExerciseAnalyzer, AnalysisResult, RepData } from './IExerciseAnalyzer';

class SquatAnalyzer implements IExerciseAnalyzer {
  public readonly name = 'squat';

  private repCount = 0;
  private stage: 'up' | 'down' = 'up';
  private feedback = '';
  private minKneeAngleThisRep = 180;
  private lastRepTimestamp = 0;

  private readonly KNEE_ANGLE_UP_THRESHOLD = 165;
  private readonly KNEE_ANGLE_DOWN_THRESHOLD = 90;

  public start(): void { this.lastRepTimestamp = Date.now(); }
  public reset(): void { this.repCount = 0; this.stage = 'up'; this.feedback = ''; this.minKneeAngleThisRep = 180; this.lastRepTimestamp = 0; }

  public analyze(landmarks: NormalizedLandmark[]): AnalysisResult {
    this.feedback = '';
    let newRepData: RepData | undefined = undefined;

    const [L_H, R_H, L_K, R_K, L_A, R_A] = [23, 24, 25, 26, 27, 28];

    const leftKneeAngle = calculateAngle(landmarks[L_H], landmarks[L_K], landmarks[L_A]);
    const rightKneeAngle = calculateAngle(landmarks[R_H], landmarks[R_K], landmarks[R_A]);

    if (this.stage === 'down') {
      this.minKneeAngleThisRep = Math.min(this.minKneeAngleThisRep, leftKneeAngle, rightKneeAngle);
    }

    if (this.stage === 'up' && leftKneeAngle < this.KNEE_ANGLE_DOWN_THRESHOLD && rightKneeAngle < this.KNEE_ANGLE_DOWN_THRESHOLD) {
      this.stage = 'down';
      this.minKneeAngleThisRep = 180;
    }
    else if (this.stage === 'down' && leftKneeAngle > this.KNEE_ANGLE_UP_THRESHOLD && rightKneeAngle > this.KNEE_ANGLE_UP_THRESHOLD) {
      this.stage = 'up';
      this.repCount++;
      this.feedback = '完成一次！';
      const now = Date.now();
      const tempo = this.lastRepTimestamp === 0 ? 0 : (now - this.lastRepTimestamp) / 1000;
      this.lastRepTimestamp = now;
      newRepData = {
        timestamp: now,
        rangeOfMotion: this.minKneeAngleThisRep,
        tempo: parseFloat(tempo.toFixed(2)),
      };
    }

    // 【纠错反馈生成点】
    if (this.stage === 'down') {
      if (landmarks[L_H].y > landmarks[L_K].y || landmarks[R_H].y > landmarks[R_K].y) {
        // 确保这里生成的字符串，与 feedbackMap.ts 中的键完全一致
        this.feedback = '蹲得再深一点！';
      }
    }

    return { repCount: this.repCount, feedback: this.feedback, stage: this.stage, newRepData };
  }
}

export const squatAnalyzer = new SquatAnalyzer();
