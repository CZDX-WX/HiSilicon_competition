import { calculateAngle } from '@/utils/vectorUtils';
import type { NormalizedLandmark } from '@mediapipe/tasks-vision';
import type { IExerciseAnalyzer, AnalysisResult, RepData } from './IExerciseAnalyzer';

class BicepCurlAnalyzer implements IExerciseAnalyzer {
  public readonly name = 'bicep_curl';

  private repCount = 0;
  private stage: 'down' | 'up' = 'down';
  private feedback = '';

  // --- 新增内部状态 ---
  private minElbowAngleThisRep = 180;
  private lastRepTimestamp = 0;

  private readonly ELBOW_ANGLE_UP_THRESHOLD = 70;
  private readonly ELBOW_ANGLE_DOWN_THRESHOLD = 160;
  // 新增 start 方法
  public start(): void {
    // 当训练开始时，将当前时间作为计时的起点
    this.lastRepTimestamp = Date.now();
  }
  public reset(): void {
    this.repCount = 0;
    this.stage = 'down';
    this.feedback = '';
    this.minElbowAngleThisRep = 180;
    this.lastRepTimestamp = 0;
  }

  public analyze(landmarks: NormalizedLandmark[]): AnalysisResult {
    this.feedback = '';
    let newRepData: RepData | undefined = undefined;

    const RIGHT_SHOULDER = 12, RIGHT_ELBOW = 14, RIGHT_WRIST = 16;
    const rightElbowAngle = calculateAngle(landmarks[RIGHT_SHOULDER], landmarks[RIGHT_ELBOW], landmarks[RIGHT_WRIST]);

    if (this.stage === 'up') {
      // 在举起阶段，持续更新本次的最小手肘角度
      this.minElbowAngleThisRep = Math.min(this.minElbowAngleThisRep, rightElbowAngle);
    }

    if (this.stage === 'down' && rightElbowAngle < this.ELBOW_ANGLE_UP_THRESHOLD) {
      this.stage = 'up';
      // 进入举起阶段时，重置本次的最小角度记录
      this.minElbowAngleThisRep = 180;
    }
    else if (this.stage === 'up' && rightElbowAngle > this.ELBOW_ANGLE_DOWN_THRESHOLD) {
      this.stage = 'down';
      this.repCount++;
      this.feedback = '完成一次！';

      const now = Date.now();
      const tempo = this.lastRepTimestamp === 0 ? 0 : (now - this.lastRepTimestamp) / 1000;
      this.lastRepTimestamp = now;

      newRepData = {
        timestamp: now,
        rangeOfMotion: this.minElbowAngleThisRep,
        tempo: parseFloat(tempo.toFixed(2)),
      };
      console.info(`%c[BicepCurl] 完成一次弯举! 幅度: ${newRepData.rangeOfMotion.toFixed(1)}°, 耗时: ${newRepData.tempo}s`, 'color: #007bff; font-weight: bold;');
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

export const bicepCurlAnalyzer = new BicepCurlAnalyzer();
