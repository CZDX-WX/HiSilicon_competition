import type { HisiCommandKey } from './commandMap';

/**
 * 将运动分析器生成的反馈文本，映射到具体的语音指令Key
 */
export const feedbackToCommandMap: Record<string, HisiCommandKey> = {
  '蹲得再深一点！': 'SPEECH_FORM_SQUAT_TOO_SHALLOW',
  '双臂再举高一点！': 'SPEECH_FORM_ARMS_NOT_HIGH_ENOUGH',
  '双腿再打开一些！': 'SPEECH_FORM_LEGS_NOT_WIDE_ENOUGH',
  '再举高一点！': 'SPEECH_FORM_BICEP_TOO_LOW',
  '手臂要伸直哦！': 'SPEECH_FORM_BICEP_NOT_STRAIGHT',
};
