import { defineStore } from 'pinia';
import { ref, watch } from 'vue';

const USER_STORAGE_KEY = 'fitness-app-user-profile';

export const useUserStore = defineStore('user', () => {
  // --- State ---
  const weight = ref(70); // 默认体重 70kg
  const height = ref(175); // 新增：默认身高 175cm

  // --- 持久化逻辑 ---
  const loadProfile = () => {
    const savedProfile = localStorage.getItem(USER_STORAGE_KEY);
    if (savedProfile) {
      try {
        const profile = JSON.parse(savedProfile);
        if (typeof profile.weight === 'number') {
          weight.value = profile.weight;
        }
        if (typeof profile.height === 'number') {
          height.value = profile.height;
        }
      } catch (e) { console.error('加载用户配置失败:', e); }
    }
  };

  const saveProfile = () => {
    const profileToSave = JSON.stringify({
      weight: weight.value,
      height: height.value,
    });
    localStorage.setItem(USER_STORAGE_KEY, profileToSave);
  };

  // 初始化时加载一次
  loadProfile();

  // 使用 watch 监听多个状态的变化并自动保存
  watch([weight, height], saveProfile, { deep: true });

  return {
    weight,
    height,
  };
});
