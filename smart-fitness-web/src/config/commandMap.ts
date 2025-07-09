/**
 * 【最终版】定义所有从前端发送到海思板的指令
 * key: 清晰的指令名称 (供我们前端开发使用)
 * value: 与海思板约定的数字枚举值
 */
export const HisiCommand = {
  // --- 系统控制指令 ---
  STOP_GESTURE_RECOGNITION: 0, // 结束校准，让板子准备推流
  RESET_BOARD_STATE: 1,        // 训练完全结束，让板子回到初始手势识别状态

  // --- 语音播报指令：校准与流程 ---
  SPEECH_CALIBRATION_LOCKED: 2,    // “姿态已锁定！请说‘准备好了’...”
  SPEECH_CALIBRATION_SUCCESS: 3,   // “收到！系统已就绪...”
  SPEECH_WORKOUT_SELECTED: 4,      // “已选择运动” (这是一个通用指令，板子可以播报“已选择运动”)
  SPEECH_WORKOUT_START: 5,         // “训练开始！”
  SPEECH_WORKOUT_PAUSE: 6,         // “训练已暂停。”
  SPEECH_WORKOUT_END: 7,           // “训练结束！你真棒！”
  SPEECH_COMMAND_NOT_RECOGNIZED: 8,// “抱歉，我没听懂您的指令”
  SPEECH_ERROR_NO_WORKOUT: 9,      // “请先选择一个运动项目”

  // --- 语音播报指令：次数里程碑 ---
  SPEECH_REP_MILESTONE_5: 10,      // “已经完成5个了”
  SPEECH_REP_MILESTONE_10: 11,     // “已经完成10个了”
  SPEECH_REP_MILESTONE_15: 12,     // “已经完成15个了”
  SPEECH_REP_MILESTONE_20: 13,     // “已经完成20个了”

  // --- 语音播报指令：动作纠错 ---
  SPEECH_FORM_SQUAT_TOO_SHALLOW: 20, // “蹲得再深一点！”
  SPEECH_FORM_ARMS_NOT_HIGH_ENOUGH: 21, // “双臂再举高一点！”
  SPEECH_FORM_LEGS_NOT_WIDE_ENOUGH: 22, // “双腿再打开一些！”
  SPEECH_FORM_BICEP_TOO_LOW: 23,        // "再举高一点！" (弯举)
  SPEECH_FORM_BICEP_NOT_STRAIGHT: 24,  // "手臂要伸直哦！" (弯举)


} as const;

export type HisiCommandKey = keyof typeof HisiCommand;
