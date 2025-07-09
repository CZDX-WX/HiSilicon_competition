import { defineStore } from 'pinia'
import { ref, computed, watch } from 'vue'
import { useUserStore } from './useUserStore'
import { isMediaPipeInitialized } from '@/composables/useMediaPipe'
import { exerciseService, type WorkoutType } from '@/services/exerciseService'
import type { IExerciseAnalyzer, RepData } from '@/services/exercises/IExerciseAnalyzer'
import { useSummaryStore } from './summaryStore'
import { useHistoryStore } from './historyStore'
import type { WorkoutRecord } from '@/types/fitness.d'
import type { NormalizedLandmark } from '@mediapipe/tasks-vision'
import { isInitialPoseValid } from '@/utils/landmarkUtils' // <-- 只引入这一个校验函数
import { speechService } from '@/services/speechService'
import { voiceControlService, type VoiceStatus } from '@/services/voiceControlService'
import { webSocketService } from '@/services/websocketService'
import { useLogStore  } from './useLogStore';
import {
  calibrationCommands,
  voiceCommands,
  workoutSelectionCommands,
} from '@/config/voiceCommands'
import { HisiCommand, type HisiCommandKey } from '@/config/commandMap'
import { feedbackToCommandMap } from '@/config/feedbackMap'
import { BROWSER_SPEECH_ENABLED } from '@/config'

export type SystemStatus =
  | 'initializing'
  | 'searching'
  | 'waiting_confirmation'
  | 'ready'
  | 'showing_report'

export type ExerciseState = 'idle' | 'running' | 'paused'

const METS_MAP: Record<WorkoutType, number> = {
  squat: 5.0,
  bicep_curl: 3.5,
  lateral_raise: 3.5,
  overhead_press: 4.0,
  jumping_jack: 8.0,
  lunge: 4.5,
  front_kick: 4.0,
}

export const useWorkoutStore = defineStore('workout', () => {
  const userStore = useUserStore()
  const systemStatus = ref<SystemStatus>('initializing')
  const exerciseState = ref<ExerciseState>('idle')
  const availableWorkouts = ref<WorkoutType[]>(exerciseService.getAvailableWorkouts())
  const currentWorkout = ref<WorkoutType | null>(null)
  const repCount = ref(0)
  const timer = ref(0)
  const feedback = ref('正在初始化AI引擎...')
  const calories = ref(0)
  const sessionRepDetails = ref<RepData[]>([])
  const voiceStatus = ref<VoiceStatus>('inactive')
  const logStore = useLogStore(); // 获取日志中心的实例
  let timerInterval: number | null = null
  let activeAnalyzer: IExerciseAnalyzer | null = null
  let lastSpokenFeedback = ''

  const formattedTimer = computed(() => {
    const hours = Math.floor(timer.value / 3600)
      .toString()
      .padStart(2, '0')
    const minutes = Math.floor((timer.value % 3600) / 60)
      .toString()
      .padStart(2, '0')
    const seconds = (timer.value % 60).toString().padStart(2, '0')
    return `${hours}:${minutes}:${seconds}`
  })

  watch(systemStatus, (newStatus, oldStatus) => {
    if (newStatus !== oldStatus) {
      console.log(
        `%c[SYSTEM] 状态变更: ${oldStatus} -> ${newStatus}`,
        'color: #28a745; font-weight: bold;',
      )
    }
  })

  const speakAndSendCommand = (
    textForBrowser: string,
    commandKey: HisiCommandKey,
    interrupt = true,
  ) => {
    const commandValue = HisiCommand[commandKey];

    // --- 核心新增：向日志中心汇报 ---
    logStore.addLog({
      status: systemStatus.value,
      commandKey: commandKey,
      commandValue: commandValue,
      voiceText: textForBrowser,
    });


    if (BROWSER_SPEECH_ENABLED) {
      speechService.speak(textForBrowser, interrupt)
      webSocketService.sendCommand(commandKey)
    } else {
      webSocketService.sendCommand(commandKey)
    }
  }

  const processLandmarks = (landmarks: NormalizedLandmark[] | undefined) => {
    // 【最终版状态机】
    switch (systemStatus.value) {
      case 'searching':
        feedback.value = '请以完整站姿进入画面中央...';
        if (isInitialPoseValid(landmarks)) {
          systemStatus.value = 'waiting_confirmation';
          speakAndSendCommand('姿态已锁定！请说“确认”或点击按钮。', 'SPEECH_CALIBRATION_LOCKED', true);
          activateVoiceControl();
        }
        break;
      case 'waiting_confirmation':
        feedback.value = '姿态已锁定！请说“确认”或点击按钮。';
        if (!isInitialPoseValid(landmarks)) {
          systemStatus.value = 'searching';
          voiceControlService.stopListening();
        }
        break;

      case 'ready':
        // 系统就绪后，只有在运动中才需要处理姿态
        if (exerciseState.value === 'running' && activeAnalyzer && landmarks) {
          const oldRepCount = repCount.value;
          const analysisResult = activeAnalyzer.analyze(landmarks);
          repCount.value = analysisResult.repCount;
          if (analysisResult.newRepData) sessionRepDetails.value.push(analysisResult.newRepData);
          if (analysisResult.feedback) {
            feedback.value = analysisResult.feedback;
            if (analysisResult.feedback !== lastSpokenFeedback && !analysisResult.feedback.includes("完成")) {
              const commandKey = feedbackToCommandMap[analysisResult.feedback];
              if (commandKey) speakAndSendCommand(analysisResult.feedback, commandKey, true);
              lastSpokenFeedback = analysisResult.feedback;
            }
          }
          if (repCount.value > oldRepCount && repCount.value > 0) {
            const milestoneKey = `SPEECH_REP_MILESTONE_${repCount.value}` as HisiCommandKey;
            if (HisiCommand[milestoneKey]) speakAndSendCommand(`已经完成 ${repCount.value} 个了！`, milestoneKey);
          }
        }
        break;
    }
  };

  /**
   * 【核心新增】由手动按钮调用的新Action
   * 关于命名：`confirmCalibration` 这个名字是准确的，因为它正是确认校准并进入下一步的动作。我们就沿用这个名字。
   */
  const confirmCalibration = () => {
    if (systemStatus.value !== 'waiting_confirmation') return;
    console.log('[SYSTEM] 用户手动确认校准。');

    // 1. 发送指令 "0" (锁定/停止手势识别)
    webSocketService.sendCommand('STOP_GESTURE_RECOGNITION');

    // 2. 将系统状态推进到最终的 ready
    systemStatus.value = 'ready';

    // 3. 语音和文字反馈
    const readyText = '收到！系统已就绪，请选择运动。';
    feedback.value = readyText;
    speakAndSendCommand(readyText, 'SPEECH_CALIBRATION_SUCCESS');
  };

  /**
   * 【最终完整版】激活语音控制，并定义所有指令的处理逻辑
   */
  const activateVoiceControl = () => {
    // 卫语句：防止服务已在运行时重复调用，虽然服务自身有保护，但这里更清晰
    if (voiceStatus.value === 'listening') {
      console.warn('[WorkoutStore] 语音监听已在运行，不再重复启动。');
      return;
    }

    // 定义1：当语音服务自身状态变化时，更新我们的UI状态
    const onStatusUpdate = (status: VoiceStatus) => {
      voiceStatus.value = status;
    };

    // 定义2：当语音服务识别出文本后，执行所有指令判断
    const onCommand = async (command: string) => {
      const lowerCaseCommand = command.toLowerCase().replace(/[\s，。]/g, '');
      if (!lowerCaseCommand) return;

      console.log(`[WorkoutStore] 接收到语音指令: "${lowerCaseCommand}"`);
      let commandMatched = false;

      // --- 情境一：系统正在“等待用户语音确认” ---
      if (systemStatus.value === 'waiting_confirmation') {
        if (calibrationCommands.CONFIRM.some(k => lowerCaseCommand.includes(k))) {
          commandMatched = true;
          console.log('[VoiceControl] 匹配到“校准确认”指令!');

          // 检查WebSocket连接是否就绪
          if (webSocketService.ws.value && webSocketService.ws.value.readyState === WebSocket.OPEN) {
            // 发送系统指令 0 (停止手势识别)
            webSocketService.sendCommand('STOP_GESTURE_RECOGNITION');

            // 切换系统状态到“准备就绪”
            systemStatus.value = 'ready';

            const readyText = '收到！系统已就绪，请选择运动。';
            feedback.value = readyText; // 更新界面反馈
            speakAndSendCommand(readyText, 'SPEECH_CALIBRATION_SUCCESS'); // 同时播报语音和发送指令
          } else {
            console.error('[VoiceControl] WebSocket未连接，指令无法发送。');
            speechService.speak("网络连接故障，请稍后重试。", true); // 使用本地语音播报错误
          }
        }
        // 在等待确认阶段，无论是否匹配，都直接返回，不处理后续运动指令
        return;
      }

      // --- 情境二：系统已“准备就绪”，处理所有运动相关指令 ---
      if (systemStatus.value === 'ready') {
        const { voiceCommands, workoutSelectionCommands } = await import('@/config/voiceCommands');

        // 优先匹配“选择运动”的指令
        for (const key in workoutSelectionCommands) {
          const workoutType = key as WorkoutType;
          if (workoutSelectionCommands[workoutType].some(k => lowerCaseCommand.includes(k))) {
            commandMatched = true;
            selectWorkout(workoutType);
            // 如果指令中也包含“开始”，则直接开始训练
            if (voiceCommands.START.some(startCmd => lowerCaseCommand.includes(startCmd))) {
              startWorkout();
            }
            break; // 匹配到就不再继续检查
          }
        }

        // 如果没有匹配到选择运动的指令，再检查“开始/暂停/结束”等通用指令
        if (!commandMatched) {
          if (voiceCommands.START.some(k => lowerCaseCommand.includes(k))) { commandMatched = true; startWorkout(); }
          else if (voiceCommands.STOP.some(k => lowerCaseCommand.includes(k))) { commandMatched = true; stopWorkout(); }
          else if (voiceCommands.PAUSE.some(k => lowerCaseCommand.includes(k))) { commandMatched = true; pauseWorkout(); }
          else if (voiceCommands.RESUME.some(k => lowerCaseCommand.includes(k))) { commandMatched = true; startWorkout(); }
        }
      }

      // // --- 收尾：如果所有情境下都没有匹配到任何指令 ---
      // if (!commandMatched && systemStatus.value !== 'searching') {
      //   const text = "抱歉，我没听懂您的指令";
      //   feedback.value = text;
      //   speakAndSendCommand(text, 'SPEECH_COMMAND_NOT_RECOGNIZED', true);
      //   console.warn(`[VoiceControl] 未匹配到任何指令: "${lowerCaseCommand}"`);
      // }
    };

    // 将我们定义好的两个“行为”函数，传递给语音服务，并启动它
    voiceControlService.startListening({ onCommand, onStatusUpdate });
  };

  const initialize = () => {
    if (isMediaPipeInitialized.value) {
      systemStatus.value = 'searching'
      feedback.value = '正在寻找用户...'
    } else {
      setTimeout(initialize, 100)
    }
  }

  const selectWorkout = (workout: WorkoutType | null) => {
    if (exerciseState.value === 'idle' && systemStatus.value === 'ready') {
      currentWorkout.value = workout
      if (workout) {
        const workoutNameMap: Record<WorkoutType, string> = {
          squat: '深蹲',
          jumping_jack: '开合跳',
          bicep_curl: '二头弯举',
          lateral_raise: '侧平举',
          overhead_press: '过头推举',
          lunge: '弓步',
          front_kick: '前踢腿',
        }
        activeAnalyzer = exerciseService.getAnalyzer(workout) || null
        activeAnalyzer?.reset()
        const text = `已选择: ${workoutNameMap[workout] || workout}。`
        feedback.value = text
        speakAndSendCommand(text, 'SPEECH_WORKOUT_SELECTED')
      } else {
        activeAnalyzer = null
      }
    }
  }

  const startWorkout = () => {
    if (systemStatus.value !== 'ready') {
      return
    }
    if (!currentWorkout.value) {
      speakAndSendCommand('请先选择一个运动项目', 'SPEECH_ERROR_NO_WORKOUT', true)
      return
    }
    if (exerciseState.value === 'idle' || exerciseState.value === 'paused') {
      if (exerciseState.value === 'idle') {
        repCount.value = 0
        timer.value = 0
        calories.value = 0
        sessionRepDetails.value = []
        activeAnalyzer?.reset()
        activeAnalyzer?.start()
      }
      exerciseState.value = 'running'
      speakAndSendCommand('训练开始！', 'SPEECH_WORKOUT_START')
      if (!timerInterval) {
        timerInterval = window.setInterval(() => {
          timer.value++
          const mets = currentWorkout.value ? METS_MAP[currentWorkout.value] : 3.0
          const hours = timer.value / 3600
          const calculatedKcal = mets * userStore.weight * hours
          calories.value = parseFloat(calculatedKcal.toFixed(1))
        }, 1000)
      }
    }
  }

  const pauseWorkout = () => {
    if (exerciseState.value !== 'running') return
    exerciseState.value = 'paused'
    speakAndSendCommand('训练已暂停。', 'SPEECH_WORKOUT_PAUSE')
    if (timerInterval) {
      clearInterval(timerInterval)
      timerInterval = null
    }
  }

  const stopWorkout = () => {
    if (exerciseState.value === 'idle') return
    // pauseWorkout()
    const summaryStore = useSummaryStore()
    const historyStore = useHistoryStore()
    const record: WorkoutRecord = {
      id: new Date().toISOString(),
      workoutType: currentWorkout.value || '未知运动',
      endDate: new Date().toLocaleString('zh-CN'),
      totalReps: repCount.value,
      totalTime: formattedTimer.value,
      totalCalories: calories.value,
      repDetails: sessionRepDetails.value,
    }
    summaryStore.show(record)
    historyStore.addRecord(record)
    speakAndSendCommand(`本组训练结束！`, 'SPEECH_WORKOUT_END')
    voiceControlService.stopListening()
    systemStatus.value = 'showing_report'
    exerciseState.value = 'idle'
    feedback.value = '查看您的本组成绩，关闭后可重新校准。'
  }

  const endReportAndPrepareForNextSet = () => {
    if (systemStatus.value !== 'showing_report') return
    activateVoiceControl()
    voiceStatus.value = 'inactive'
    systemStatus.value = 'searching'
    currentWorkout.value = null
    activeAnalyzer = null
    feedback.value = '请重新进入画面以开始新的训练。'
    speakAndSendCommand(feedback.value,'RESET_BOARD_STATE',true)

  }

  const resetSession = () => {
    activateVoiceControl()
    voiceStatus.value = 'inactive'
    systemStatus.value = 'searching'
    exerciseState.value = 'idle'
    currentWorkout.value = null
    activeAnalyzer = null
    feedback.value = '请重新进入画面以开始新的训练。'
    speakAndSendCommand(feedback.value,'RESET_BOARD_STATE',true)
  }

  return {
    systemStatus,
    exerciseState,
    availableWorkouts,
    currentWorkout,
    repCount,
    timer,
    feedback,
    calories,
    formattedTimer,
    sessionRepDetails,
    voiceStatus,
    initialize,
    processLandmarks,
    confirmCalibration,
    selectWorkout,
    startWorkout,
    pauseWorkout,
    stopWorkout,
    endReportAndPrepareForNextSet,
    resetSession,
  }
})
