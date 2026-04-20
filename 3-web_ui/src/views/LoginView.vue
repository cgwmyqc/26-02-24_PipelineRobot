<template>
  <div class="login-page">
    <div class="login-card">
      <div class="login-brand">
        <img class="brand-badge" :src="loginBrand" alt="品牌标识" />
        <div>
          <p class="eyebrow">Pipeline Robot Digital Twin</p>
          <h1>低干扰排水管道缺陷与状态自动检测装置智能监测平台</h1>
        </div>
      </div>

      <el-form :model="form" class="login-form" @submit.prevent="handleLogin">
        <el-form-item>
          <el-input v-model="form.username" placeholder="账号" size="large" />
        </el-form-item>
        <el-form-item>
          <el-input v-model="form.password" type="password" placeholder="密码" size="large" show-password />
        </el-form-item>
        <el-form-item>
          <el-button type="success" size="large" class="submit-btn" :loading="authStore.loading" @click="handleLogin">
            登录系统
          </el-button>
        </el-form-item>
      </el-form>

      <div class="login-hint">
        <span>默认账号由后端初始化数据提供</span>
      </div>
    </div>
  </div>
</template>

<script setup>
import { reactive } from 'vue'
import { ElMessage } from 'element-plus'
import { useRouter } from 'vue-router'
import { useAuthStore } from '../stores/auth'
import loginBrand from '../assets/images/login_brand.png'

const router = useRouter()
const authStore = useAuthStore()

const form = reactive({
  username: 'admin',
  password: 'admin123'
})

async function handleLogin() {
  if (!form.username || !form.password) {
    ElMessage.warning('请输入账号和密码')
    return
  }

  try {
    await authStore.login({ ...form })
    router.push('/')
  } catch (error) {
    ElMessage.error(error?.response?.data?.message || '登录失败')
  }
}
</script>

<style scoped>
.login-page {
  min-height: 100vh;
  display: grid;
  place-items: center;
  padding: 24px;
}

.login-card {
  width: min(500px, 100%);
  padding: 36px;
  border: 1px solid rgba(103, 212, 255, 0.18);
  background:
    radial-gradient(circle at top left, rgba(117, 240, 194, 0.18), transparent 32%),
    linear-gradient(180deg, rgba(18, 34, 54, 0.95), rgba(10, 24, 40, 0.92));
  box-shadow: 0 18px 48px rgba(0, 0, 0, 0.28);
}

.login-brand {
  display: grid;
  grid-template-columns: 76px 1fr;
  gap: 18px;
  align-items: center;
  margin-bottom: 28px;
}

.brand-badge {
  display: block;
  width: 80px;
  height: 80px;
  object-fit: contain;
}

.eyebrow {
  margin: 0 0 5px;
  color: var(--text-dim);
  text-transform: uppercase;
  letter-spacing: 2px;
  font-size: 12px;
}

h1 {
  margin: 0;
  font-size: 25px;
  line-height: 1.3;
}

.submit-btn {
  width: 100%;
}

.login-hint {
  color: var(--text-dim);
  font-size: 13px;
  text-align: center;
}
</style>
