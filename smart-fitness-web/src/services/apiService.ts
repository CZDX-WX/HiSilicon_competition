import type { FitnessData } from '@/types/fitness.d';

let mockRepCount = 0;
let mockSeconds = 0;
const feedbackOptions = [
  '动作非常标准！', '保持呼吸节奏。', '蹲得再深一点！', '核心收紧！', '膝盖不要内扣！',
];

export const fetchFitnessData = (): Promise<FitnessData> => {
  return new Promise((resolve) => {
    setTimeout(() => {
      mockRepCount += Math.random() > 0.7 ? 1 : 0;
      mockSeconds++;
      const minutes = Math.floor(mockSeconds / 60).toString().padStart(2, '0');
      const seconds = (mockSeconds % 60).toString().padStart(2, '0');

      const mockData: FitnessData = {
        workout_type: '深蹲',
        rep_count: mockRepCount,
        timer: {
          display: `00:${minutes}:${seconds}`,
          seconds: mockSeconds
        },
        calories: Math.round(mockRepCount * 0.35 * 10) / 10,
        feedback_text: feedbackOptions[Math.floor(Math.random() * feedbackOptions.length)],
        form_quality_score: Math.floor(Math.random() * 21) + 80,
      };
      resolve(mockData);
    }, 300);
  });
};

export const resetMockData = () => {
  mockRepCount = 0;
  mockSeconds = 0;
};
