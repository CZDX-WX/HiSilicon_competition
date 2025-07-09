import { calculateAngle } from '@/utils/vectorUtils';
import type { NormalizedLandmark } from '@mediapipe/tasks-vision';
import type { IExerciseAnalyzer, AnalysisResult, RepData } from './IExerciseAnalyzer';

class LungeAnalyzer implements IExerciseAnalyzer {
  public readonly name = 'lunge';

  private repCount = 0;
  private stage: 'up' | 'down' = 'up';
  private feedback = '';
  private lastRepTimestamp = 0;
  private minKneeAngleThisRep = 180;
  private minHipAngleThisRep = 180;

  private readonly KNEE_DOWN_THRESHOLD = 110;
  private readonly KNEE_UP_THRESHOLD = 160;

  public start(): void { this.lastRepTimestamp = Date.now(); }

  public reset(): void {
    this.repCount = 0;
    this.stage = 'up';
    this.feedback = '';
    this.lastRepTimestamp = 0;
    this.minKneeAngleThisRep = 180;
    this.minHipAngleThisRep = 180;
  }

  public analyze(landmarks: NormalizedLandmark[]): AnalysisResult {
    this.feedback = '';
    let newRepData: RepData | undefined = undefined;

    const [L_S, R_S, L_H, R_H, L_K, R_K, L_A, R_A] = [11, 12, 23, 24, 25, 26, 27, 28];

    const leftKneeAngle = calculateAngle(landmarks[L_H], landmarks[L_K], landmarks[L_A]);
    const rightKneeAngle = calculateAngle(landmarks[R_H], landmarks[R_K], landmarks[R_A]);
    const leftHipAngle = calculateAngle(landmarks[L_S], landmarks[L_H], landmarks[L_K]);
    const rightHipAngle = calculateAngle(landmarks[R_S], landmarks[R_H], landmarks[R_K]);

    if (this.stage === 'down') {
      this.minKneeAngleThisRep = Math.min(this.minKneeAngleThisRep, leftKneeAngle, rightKneeAngle);
      this.minHipAngleThisRep = Math.min(this.minHipAngleThisRep, leftHipAngle, rightHipAngle);
    }

    if (this.stage === 'up' && leftKneeAngle < this.KNEE_DOWN_THRESHOLD && rightKneeAngle < this.KNEE_DOWN_THRESHOLD) {
      this.stage = 'down';
      this.minKneeAngleThisRep = 180;
      this.minHipAngleThisRep = 180;
    } else if (this.stage === 'down' && leftKneeAngle > this.KNEE_UP_THRESHOLD && rightKneeAngle > this.KNEE_UP_THRESHOLD) {
      this.stage = 'up';
      this.repCount++;
      this.feedback = '完成一次！';

      const now = Date.now();
      const tempo = this.lastRepTimestamp === 0 ? 0 : (now - this.lastRepTimestamp) / 1000;
      this.lastRepTimestamp = now;

      newRepData = {
        timestamp: now,
        rangeOfMotion: this.minKneeAngleThisRep, // 以膝盖角度作为幅度的主要衡量
        tempo: parseFloat(tempo.toFixed(2)),
      };
    }

    if(this.stage === 'down') {
      if(leftKneeAngle > 120 || rightKneeAngle > 120) {
        this.feedback = '下蹲再深一些！';
      }
    }

    return { repCount: this.repCount, feedback: this.feedback, stage: this.stage, newRepData };
  }
}

export const lungeAnalyzer = new LungeAnalyzer();
