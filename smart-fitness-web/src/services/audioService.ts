// 定义一个映射，将声音名称映射到文件路径
const soundMap = {
  rep: '/sounds/rep_sound.mp3',
  alert: '/sounds/alert_sound.mp3',
};

type SoundName = keyof typeof soundMap;

const audioInstances: { [key in SoundName]?: HTMLAudioElement } = {};

// 预加载音频，避免首次播放延迟
export const initAudio = () => {
  (Object.keys(soundMap) as SoundName[]).forEach(name => {
    if (!audioInstances[name]) {
      audioInstances[name] = new Audio(soundMap[name]);
    }
  });
  console.log('Audio service initialized.');
};

// 播放指定名称的声音
export const playSound = (name: SoundName) => {
  const audio = audioInstances[name];
  if (audio) {
    // 将播放时间重置到开头，以允许快速连续播放
    audio.currentTime = 0;
    audio.play().catch(error => console.error(`Error playing sound ${name}:`, error));
  } else {
    console.warn(`Sound not found: ${name}`);
  }
};
