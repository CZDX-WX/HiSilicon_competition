<script setup lang="ts">
import { useSummaryStore } from '@/stores/summaryStore';
import { Line, Bar } from 'vue-chartjs';
import {
  Chart as ChartJS,
  Title, Tooltip, Legend, Filler,
  LineElement, PointElement, BarElement,
  CategoryScale, LinearScale,
} from 'chart.js';
import { computed } from 'vue';
import type { WorkoutType } from '@/services/exerciseService';
import { useWorkoutStore } from '@/stores/workoutStore';

const workoutStore = useWorkoutStore(); // 2. 获取实例
/**
 * 【核心修改】创建一个新的关闭处理函数
 */
const handleConfirmAndClose = () => {
  // a. 调用 workoutStore 的方法来重置系统
  workoutStore.endReportAndPrepareForNextSet();
  // b. 调用 summaryStore 的方法来关闭弹窗本身
  summaryStore.hide();
};

ChartJS.register(Title, Tooltip, Legend, Filler, LineElement, PointElement, BarElement, CategoryScale, LinearScale);

const summaryStore = useSummaryStore();
const workoutData = computed(() => summaryStore.summaryData);

// --- 图表数据准备 ---

// 1. 动作幅度 (ROM) 图表数据
const romChartData = computed(() => {
  const details = workoutData.value?.repDetails ?? [];
  const workoutType = workoutData.value?.workoutType as WorkoutType;
  const romValues = details.map(d => {
    // 对于角度越小越好的动作（深蹲、弯举），取负值以便图表上“柱子更低”
    if (workoutType === 'squat' || workoutType === 'bicep_curl') {
      // 返回一个正值，但在options中设置 reverse: true 会更好，但Chart.js v4中配置复杂
      // 这里我们约定，图表标题会解释数值含义
      return d.rangeOfMotion;
    }
    return d.rangeOfMotion;
  });

  return {
    labels: details.map((_, index) => `第 ${index + 1} 次`),
    datasets: [{
      label: '动作幅度',
      backgroundColor: 'rgba(88, 166, 255, 0.6)',
      borderColor: '#58A6FF',
      borderWidth: 1,
      borderRadius: 4,
      data: romValues,
    }],
  };
});

// 2. 动作节奏 (Tempo) 图表数据
const tempoChartData = computed(() => {
  const details = workoutData.value?.repDetails ?? [];
  // 忽略第一次的耗时，因为它不代表节奏
  const tempoValues = details.map(d => d.tempo).slice(1);
  return {
    labels: details.slice(1).map((_, index) => `第 ${index + 2} 次`),
    datasets: [{
      label: '动作耗时',
      borderColor: '#3FB950',
      pointBackgroundColor: '#3FB950',
      pointBorderColor: '#fff',
      data: tempoValues,
      tension: 0.4,
      fill: true,
      backgroundColor: (context) => {
        const ctx = context.chart.ctx;
        const gradient = ctx.createLinearGradient(0, 0, 0, 200);
        gradient.addColorStop(0, 'rgba(63, 185, 80, 0.5)');
        gradient.addColorStop(1, 'rgba(63, 185, 80, 0)');
        return gradient;
      },
    }],
  };
});


// --- 图表选项 ---

const romChartOptions = computed(() => {
  const workoutType = workoutData.value?.workoutType as WorkoutType;
  const romTitleMap: Partial<Record<WorkoutType, string>> = {
    squat: '深度 (角度越小越深)',
    bicep_curl: '弯举幅度 (角度越小越高)',
    lateral_raise: '抬升高度 (角度越大越高)',
    overhead_press: '推举高度 (Y坐标越小越高)',
    jumping_jack: '开合幅度 (比例越大越开)'
  };
  const yAxisTitle = romTitleMap[workoutType] || '动作幅度';

  return {
    responsive: true,
    maintainAspectRatio: false,
    plugins: { legend: { display: false }, tooltip: { backgroundColor: 'rgba(0, 0, 0, 0.8)' } },
    scales: {
      y: { title: { display: true, text: yAxisTitle, color: '#8B949E' }, grid: { color: 'rgba(139, 148, 158, 0.2)' }, ticks: { color: '#8B949E' } },
      x: { grid: { display: false }, ticks: { color: '#8B949E' } },
    },
  }
});

const tempoChartOptions = computed(() => ({
  responsive: true, maintainAspectRatio: false,
  plugins: { legend: { display: false }, tooltip: { backgroundColor: 'rgba(0, 0, 0, 0.8)' } },
  scales: {
    y: {
      title: { display: true, text: '耗时(秒)', color: '#8B949E' },
      grid: { color: 'rgba(139, 148, 158, 0.2)' },
      ticks: { color: '#8B949E' },
      suggestedMin: 0,
      suggestedMax: 10,
    },
    x: { grid: { display: false }, ticks: { color: '#8B949E' } },
  },
}));

// --- 【核心新增】生成可改善建议的计算属性 ---
const improvementSuggestions = computed((): string[] => {
  const details = workoutData.value?.repDetails;
  if (!details || details.length < 3) {
    return ["数据量过少，无法生成有效建议。"];
  }

  const suggestions: string[] = [];
  const repCount = details.length;

  // 1. 分析动作幅度 (ROM)
  const roms = details.map(d => d.rangeOfMotion);
  const avgRom = roms.reduce((a, b) => a + b, 0) / repCount;
  const romStdDev = Math.sqrt(roms.map(x => Math.pow(x - avgRom, 2)).reduce((a, b) => a + b, 0) / repCount);
  if (avgRom !== 0 && (romStdDev / avgRom) > 0.15) { // 幅度标准差占平均值的比例，如果大于15%，认为不稳定
    suggestions.push("动作幅度一致性有待提高。请尝试让每一次动作都达到相似的深度或高度，确保训练效果均衡。");
  }

  // 2. 分析动作节奏 (Tempo)
  const tempos = details.map(d => d.tempo).slice(1);
  if (tempos.length > 1) {
    const avgTempo = tempos.reduce((a, b) => a + b, 0) / tempos.length;
    if (avgTempo < 1.5) { // 假设每次动作少于1.5秒算过快
      suggestions.push("动作节奏偏快。建议适当放慢，尤其在动作的离心阶段（下放），感受肌肉的控制，这能更好地刺激增长。");
    }
  }

  // 3. 分析体力趋势 (比较前后半段的幅度)
  const firstHalfRoms = roms.slice(0, Math.floor(repCount / 2));
  const secondHalfRoms = roms.slice(Math.ceil(repCount / 2));
  if (firstHalfRoms.length > 1 && secondHalfRoms.length > 1) {
    const firstHalfAvg = firstHalfRoms.reduce((a, b) => a + b, 0) / firstHalfRoms.length;
    const secondHalfAvg = secondHalfRoms.reduce((a, b) => a + b, 0) / secondHalfRoms.length;
    if (secondHalfAvg < firstHalfAvg * 0.9) { // 如果后半段平均幅度衰减超过10%
      suggestions.push("训练后半段动作幅度有衰减。这是体力消耗的正常现象，请在保证动作标准的前提下坚持，或适当安排休息。");
    }
  }

  // 如果没有任何问题，则给一条鼓励
  if (suggestions.length === 0) {
    suggestions.push("整体表现非常棒，动作标准且稳定，请继续保持！");
  }

  return suggestions;
});
</script>

<template>
  <Transition name="modal-fade">
    <div v-if="summaryStore.showSummary && workoutData" class="modal-overlay">
      <div class="modal-content glass-card">
        <header class="modal-header">
          <h2>运动表现分析</h2>
          <p>“{{ workoutData.workoutType }}” 训练总结</p>
        </header>

        <div class="stats-grid">
          <div class="stat-item"><span>总次数</span><strong>{{ workoutData.totalReps }}</strong></div>
          <div class="stat-item"><span>总时长</span><strong>{{ workoutData.totalTime }}</strong></div>
          <div class="stat-item"><span>总消耗 (千卡)</span><strong>{{ workoutData.totalCalories.toFixed(1) }}</strong></div>
        </div>

        <div class="charts-grid">
          <div class="chart-wrapper">
            <div class="chart-title">动作幅度(ROM)分析</div>
            <Bar :data="romChartData" :options="romChartOptions" :height="200"/>
          </div>
          <div class="chart-wrapper">
            <div class="chart-title">动作节奏(Tempo)分析</div>
            <Line :data="tempoChartData" :options="tempoChartOptions" :height="200"/>
          </div>
        </div>

        <div v-if="improvementSuggestions.length > 0" class="suggestions-box glass-card">
          <div class="chart-title">AI 训练建议</div>
          <ul>
            <li v-for="(suggestion, index) in improvementSuggestions" :key="index">
              {{ suggestion }}
            </li>
          </ul>
        </div>

        <button @click="handleConfirmAndClose" class="btn close-btn">
          好的，知道了
        </button>
      </div>
    </div>
  </Transition>
</template>

<style scoped>
.glass-card {
  background: rgba(30, 36, 44, 0.8);
  backdrop-filter: blur(15px);
  border: 1px solid var(--color-border);
  border-radius: 16px;
}

.modal-overlay {
  position: fixed; top: 0; left: 0; width: 100%; height: 100%;
  background-color: rgba(0, 0, 0, 0.7);
  display: flex; justify-content: center; align-items: center; z-index: 1000;
  padding: 20px 0;
}
.modal-content {
  width: 90%;
  max-width: 800px;
  max-height: 95vh;
  overflow-y: auto;
  color: var(--color-text-primary);
  padding: 32px;
  display: flex; flex-direction: column; gap: 24px;
}

.modal-header { text-align: center; }
.modal-header h2 { margin: 0; font-size: 2rem; color: var(--color-accent-primary); }
.modal-header p { margin: 4px 0 0; color: var(--color-text-secondary); }

.stats-grid { display: flex; justify-content: space-around; text-align: center; gap: 16px; }
.stat-item { background: var(--color-background-soft); padding: 16px; border-radius: 8px; flex-grow: 1; }
.stat-item span { display: block; font-size: 0.9rem; color: var(--color-text-secondary); margin-bottom: 8px; }
.stat-item strong { font-family: var(--font-display); font-size: 1.8rem; }

.charts-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 24px;
  align-items: start;
}
.chart-wrapper {
  background: var(--color-background-soft);
  padding: 16px;
  border-radius: 8px;
  display: flex;
  flex-direction: column;
  height: 200px;         /* 与组件 height 一致 */
  position: relative;
}
.chart-title {
  max-height: 100% !important;
  color: var(--color-text-secondary);
  text-align: center;
  margin-bottom: 12px;
  font-size: 0.9rem;
  font-weight: 700;
}

.suggestions-box {
  padding: 16px 24px;
  border: 1px solid var(--color-accent-secondary);
}
.suggestions-box ul {
  margin: 0;
  padding-left: 20px;
  display: flex;
  flex-direction: column;
  gap: 12px;
}
.suggestions-box li {
  color: var(--color-text-primary);
  line-height: 1.6;
}

.btn { padding: 12px; border: none; border-radius: 6px; font-size: 1rem; font-family: var(--font-display); font-weight: 700; cursor: pointer; transition: all 0.2s ease-in-out; width: 100%; }
.close-btn { background-image: linear-gradient(to right, var(--color-accent-primary) 0%, #3FB950 100%); color: #fff; }
.close-btn:hover { box-shadow: 0 0 20px rgba(88, 166, 255, 0.5); }

.modal-fade-enter-active, .modal-fade-leave-active { transition: all 0.3s ease; }
.modal-fade-enter-from, .modal-fade-leave-to { opacity: 0; transform: scale(0.95); }
</style>
