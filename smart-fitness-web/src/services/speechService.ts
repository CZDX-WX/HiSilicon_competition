import { ref } from 'vue';

class SpeechService {
  private synthesis: SpeechSynthesis;
  private voices = ref<SpeechSynthesisVoice[]>([]);
  private selectedVoice: SpeechSynthesisVoice | null = null;
  // 不再需要 isReady 标志位，我们将让服务更有弹性

  constructor() {
    // 检查浏览器是否支持
    if (!('speechSynthesis' in window)) {
      console.error('抱歉，您的浏览器不支持语音合成功能。');
      this.synthesis = null!;
      return;
    }
    this.synthesis = window.speechSynthesis;

    // 尝试加载语音列表。这是一个异步过程，我们不再阻塞 speak 方法
    this.populateVoiceList();
    if (this.synthesis.onvoiceschanged !== undefined) {
      this.synthesis.onvoiceschanged = this.populateVoiceList.bind(this);
    }
  }

  /**
   * 加载并筛选可用的语音列表
   */
  private populateVoiceList(): void {
    const availableVoices = this.synthesis.getVoices();
    if (availableVoices.length === 0) {
      // 某些浏览器需要一点时间，稍后重试
      setTimeout(() => this.populateVoiceList(), 100);
      return;
    }

    this.voices.value = availableVoices;
    this.selectedVoice =
      this.voices.value.find(v => v.lang.includes('zh-CN') && v.name.includes('Huihui')) ||
      this.voices.value.find(v => v.lang.includes('zh-CN')) ||
      this.voices.value.find(v => v.default) || // 降级到默认语音
      null;

    console.log('✅ 语音播报服务已就绪, 候选声音列表已加载。');
    console.log('自动选择的语音:', this.selectedVoice?.name || '系统默认');
  }

  /**
   * 【已重构】核心方法：说出指定的文本
   * @param text - 要播报的文本内容
   * @param interrupt - 是否要打断当前正在进行的播报
   */
  public speak(text: string, interrupt: boolean = false): void {
    // 关键修复：不再检查 isReady，只要 synthesis 存在就尝试播报
    if (!this.synthesis || !text) {
      console.warn(`语音引擎不存在或文本为空，无法播报: "${text}"`);
      return;
    }

    if (interrupt) {
      this.synthesis.cancel();
    }

    const utterance = new SpeechSynthesisUtterance(text);

    // 即使 selectedVoice 此刻仍为 null，浏览器也会使用默认语音，不会导致失败
    utterance.voice = this.selectedVoice;
    utterance.volume = 1;
    utterance.rate = 1.2;
    utterance.pitch = 1;

    console.log(`[SpeechService] 正在播报: "${text}" (使用声音: ${utterance.voice?.name || '默认'})`);
    this.synthesis.speak(utterance);
  }
}

export const speechService = new SpeechService();
