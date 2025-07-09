import type { IExerciseAnalyzer } from './exercises/IExerciseAnalyzer';

// 1. 从新文件中导入我们刚刚创建的分析器
import { squatAnalyzer } from './exercises/squat';
import { jumpingJackAnalyzer } from './exercises/jumpingJack.ts';
import { bicepCurlAnalyzer } from './exercises/bicepCurl';
import { lateralRaiseAnalyzer } from './exercises/lateralRaise';
import { overheadPressAnalyzer } from './exercises/overheadPress';
import { lungeAnalyzer } from './exercises/lunge';           // <-- 新增
import { frontKickAnalyzer } from './exercises/frontKick';   // <-- 新增


/**
 * 2. 在类型定义中加入新的运动ID
 */
export type WorkoutType =
  | 'squat'
  | 'bicep_curl'
  | 'lateral_raise'
  | 'overhead_press'
  | 'jumping_jack'
  | 'lunge'           // <-- 新增
  | 'front_kick';     // <-- 新增

/**
 * 运动分析器注册表 (一个从运动名称到分析器实例的映射)
 */
const analyzers = new Map<WorkoutType, IExerciseAnalyzer>();
// 3. 将新的分析器注册到 Map 中
analyzers.set('squat', squatAnalyzer);
analyzers.set('lateral_raise', lateralRaiseAnalyzer);
analyzers.set('overhead_press', overheadPressAnalyzer);
analyzers.set('bicep_curl', bicepCurlAnalyzer);
analyzers.set('jumping_jack', jumpingJackAnalyzer);
analyzers.set('lunge', lungeAnalyzer);                 // <-- 新增
analyzers.set('front_kick', frontKickAnalyzer);     // <-- 新增

class ExerciseService {
  public getAvailableWorkouts(): WorkoutType[] {
    return Array.from(analyzers.keys());
  }

  public getAnalyzer(type: WorkoutType): IExerciseAnalyzer | undefined {
    return analyzers.get(type);
  }
}

export const exerciseService = new ExerciseService();
