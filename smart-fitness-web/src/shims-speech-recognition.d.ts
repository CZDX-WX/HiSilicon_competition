// src/shims-speech-recognition.d.ts

// 声明全局的 SpeechRecognition 接口，简化起见方法/属性都用 any，可根据需要再精细补充
interface SpeechRecognition extends EventTarget {
  lang: string;
  interimResults: boolean;
  maxAlternatives: number;
  start(): void;
  stop(): void;
  abort(): void;
  onaudiostart: ((this: SpeechRecognition, ev: Event) => any) | null;
  onresult: ((this: SpeechRecognition, ev: SpeechRecognitionEvent) => any) | null;
  onerror: ((this: SpeechRecognition, ev: SpeechRecognitionErrorEvent) => any) | null;
  // …根据需求补充其他 event handler…
}

// 兼容不同浏览器，在 window 上声明两个构造函数
interface Window {
  SpeechRecognition: {
    prototype: SpeechRecognition;
    new (): SpeechRecognition;
  };
  webkitSpeechRecognition: {
    prototype: SpeechRecognition;
    new (): SpeechRecognition;
  };
}

// 让上面声明文件成为模块，从而被 TS 捕获
export {};
