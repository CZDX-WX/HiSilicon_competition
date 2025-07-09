export type VoiceStatus = 'listening' | 'processing' | 'stopped' | 'error' | 'inactive';

interface StartListeningCallbacks {
  onCommand: (command: string) => void;
  onStatusUpdate: (status: VoiceStatus) => void;
}

class VoiceControlService {
  private recognition: SpeechRecognition | null = null;
  private isListening = false;
  private callbacks: StartListeningCallbacks | null = null;

  constructor() {
    const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
    if (!SpeechRecognition) { return; }
    this.recognition = new SpeechRecognition();
    this.recognition.continuous = true;
    this.recognition.lang = 'zh-CN';
    this.recognition.interimResults = false;
    this.setupEventListeners();
  }

  private setupEventListeners(): void {
    if (!this.recognition) return;

    this.recognition.onstart = () => {
      this.isListening = true;
      this.callbacks?.onStatusUpdate('listening');
      console.log('%c[VoiceControl] 识别服务已成功启动并正在聆听。', 'color: lightgreen;');
    };

    this.recognition.onresult = (event: SpeechRecognitionEvent) => {
      const transcript = event.results[event.results.length - 1][0].transcript.trim();
      this.callbacks?.onCommand(transcript);
    };

    this.recognition.onend = () => {
      const wasListening = this.isListening;
      this.isListening = false;
      this.callbacks?.onStatusUpdate('stopped');
      // 只有在不是我们主动调用stop()的情况下，才进行自动重启
      if (wasListening) {
        console.warn('[VoiceControl] 识别服务意外断开，准备重启...');
        this.startListening(this.callbacks!);
      } else {
        console.log('[VoiceControl] 识别服务已正常停止。');
      }
    };

    this.recognition.onerror = (event: SpeechRecognitionErrorEvent) => {
      this.callbacks?.onStatusUpdate('error');
      console.error(`[VoiceControl] 识别服务错误: ${event.error}`, event);
    };
  }

  /**
   * 【已重构】更健壮的启动方法
   */
  public startListening(callbacks: StartListeningCallbacks): void {
    if (!this.recognition) return;

    // 更新回调函数
    this.callbacks = callbacks;

    // 如果已经在听，则无需操作
    if (this.isListening) {
      console.log('[VoiceControl] startListening被调用，但已在监听中，仅更新回调。');
      return;
    }

    try {
      this.recognition.start();
    } catch (e) {
      // 捕获“已经启动”的错误，这通常意味着一个快速的 stop/start 周期
      // 在这种情况下，我们什么都不做，等待 onend 事件来处理重启
      if (e instanceof DOMException && e.name === 'InvalidStateError') {
        console.warn('[VoiceControl] 尝试启动一个已在启动过程中的识别服务，已忽略。');
      } else {
        console.error('[VoiceControl] 启动失败:', e);
      }
    }
  }

  /**
   * 【已重构】更健壮的停止方法
   */
  public stopListening(): void {
    if (!this.recognition || !this.isListening) return;

    this.isListening = false; // 明确表示我们不希望它重启
    this.recognition.stop();
  }
}

export const voiceControlService = new VoiceControlService();
