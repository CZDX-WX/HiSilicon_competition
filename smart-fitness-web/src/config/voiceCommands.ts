import type { WorkoutType } from '@/services/exerciseService';

// 定义命令和对应的关键词数组
export const voiceCommands = {
  START: ['开始训练', '开始运动', '开始','启动'],
  STOP: ['结束训练', '结束运动', '结束'],
  PAUSE: ['暂停'],
  RESUME: ['继续', '继续训练'],
};

// 定义选择运动的命令，将语音关键词映射到运动ID
export const workoutSelectionCommands: Record<WorkoutType, string[]> = {
  squat: ['深蹲', '开始深蹲'],
  bicep_curl: ['弯举', '二头弯举', '开始弯举'],
  lateral_raise: ['侧平举', '开始侧平举'],
  overhead_press: ['推举', '过头推举', '开始推举'],
  jumping_jack: ['开合跳', '开始开合跳'],
  lunge: ['弓步', '开始弓步','公布'],
  front_kick: ['前踢腿', '开始前踢腿'],
};

export const calibrationCommands = {
  CONFIRM: ['确认校准', '确认', '校准完成','准备好了'],
};

