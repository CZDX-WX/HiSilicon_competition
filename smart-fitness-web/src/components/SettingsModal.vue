<script setup lang="ts">
import { useUserStore } from '@/stores/useUserStore';
defineProps<{ modelValue: boolean }>();
const emit = defineEmits(['update:modelValue']);
const userStore = useUserStore();
const closeModal = () => {
  emit('update:modelValue', false);
};
</script>

<template>
  <Transition name="modal-fade">
    <div v-if="modelValue" class="modal-overlay" @click.self="closeModal">
      <div class="modal-content glass-card">
        <header class="modal-header">
          <h2>个人设置</h2>
          <p>您的个人数据将用于更精确地计算运动消耗</p>
        </header>
        <div class="form-group">
          <label for="weight-input">体重 (kg)</label>
          <input
            id="weight-input"
            type="number"
            v-model.number="userStore.weight"
            placeholder="例如: 70"
          />
        </div>
        <div class="form-group">
          <label for="height-input">身高 (cm)</label>
          <input
            id="height-input"
            type="number"
            v-model.number="userStore.height"
            placeholder="例如: 175"
          />
        </div>
        <button @click="closeModal" class="btn close-btn">保存并关闭</button>
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
}
.modal-content {
  width: 90%; max-width: 400px; color: var(--color-text-primary);
  padding: 32px; display: flex; flex-direction: column; gap: 24px;
}
.modal-header { text-align: center; }
.modal-header h2 { margin: 0; font-size: 1.8rem; color: var(--color-accent-primary); }
.modal-header p { margin: 4px 0 0; color: var(--color-text-secondary); }
.form-group { display: flex; flex-direction: column; gap: 8px; }
.form-group label { font-weight: 700; color: var(--color-text-secondary); }
.form-group input {
  padding: 12px; border-radius: 6px; border: 1px solid var(--color-border);
  background-color: var(--color-background-soft);
  color: var(--color-text-primary); font-size: 1rem; font-family: var(--font-primary);
}
.form-group input:focus {
  outline: none; border-color: var(--color-accent-primary);
  box-shadow: 0 0 10px rgba(88, 166, 255, 0.3);
}
.btn { padding: 12px; border: none; border-radius: 6px; font-size: 1rem; font-family: var(--font-display); font-weight: 700; cursor: pointer; transition: all 0.2s ease-in-out; width: 100%; }
.close-btn { background-image: linear-gradient(to right, var(--color-accent-primary) 0%, #3FB950 100%); color: #fff; }
.close-btn:hover { box-shadow: 0 0 20px rgba(88, 166, 255, 0.5); }
.modal-fade-enter-active, .modal-fade-leave-active { transition: all 0.3s ease; }
.modal-fade-enter-from, .modal-fade-leave-to { opacity: 0; transform: scale(0.95); }

/* --- 隐藏数字输入框的上下箭头 --- */
input[type=number]::-webkit-inner-spin-button,
input[type=number]::-webkit-outer-spin-button {
  -webkit-appearance: none;
  margin: 0;
}
input[type=number] {
  -moz-appearance: textfield;
}
</style>
