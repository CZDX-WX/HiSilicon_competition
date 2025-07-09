// 定义了从后端获取的健身数据对象的结构
export interface FitnessData {
  workout_type: string;
  rep_count: number;
  timer: {
    display: string;  // 用于显示给用户, e.g., "00:01:35"
    seconds: number;  // 总秒数, e.g., 95
  };
  calories: number;
  feedback_text: string;
  form_quality_score: number;
}

// WorkoutRecord 定义了历史记录的结构
export interface WorkoutRecord {
  id: string;
  workoutType: string;
  endDate: string;
  totalReps: number;
  totalTime: string;
  totalCalories: number;
  repDetails: RepData[]; // <-- 确保字段已更新
}
