import { createRouter, createWebHistory } from 'vue-router'
import HomeView from '../views/HomeView.vue'

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    {
      path: '/',
      name: 'workout',
      component: HomeView,
      meta: {
        title: '实时训练'
      }
    },
    {
      path: '/history',
      name: 'history',
      // 我们对“历史记录”页面使用路由懒加载。
      // 这意味着只有当用户第一次访问这个页面时，才会下载对应的代码。
      // 这对于优化应用的初始加载速度非常有帮助。
      component: () => import('../views/HistoryView.vue'),
      meta: {
        title: '历史记录'
      }
    }
  ]
})

export default router
