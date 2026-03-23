<template>
  <section class="panel history-panel">
    <div class="panel-title">历史查询</div>
    <div class="panel-body history-body">
      <el-form :inline="true" class="filter-form">
        <el-form-item>
          <el-radio-group v-model="filters.mode">
            <el-radio value="">全部</el-radio>
            <el-radio value="自动巡检">自动巡检</el-radio>
            <el-radio value="人工巡检">人工巡检</el-radio>
          </el-radio-group>
        </el-form-item>
        <el-form-item>
          <el-input v-model="filters.environment" placeholder="管道环境" clearable />
        </el-form-item>
        <el-form-item>
          <el-date-picker v-model="filters.startDate" type="date" placeholder="开始日期" value-format="YYYY-MM-DD" />
        </el-form-item>
        <el-form-item>
          <el-date-picker v-model="filters.endDate" type="date" placeholder="结束日期" value-format="YYYY-MM-DD" />
        </el-form-item>
        <el-form-item>
          <el-button type="success" @click="handleSearch">查询</el-button>
        </el-form-item>
      </el-form>

      <el-table :data="list" height="280" v-loading="loading">
        <el-table-column type="index" label="序号" width="62" />
        <el-table-column prop="mode" label="巡检方式" width="100" />
        <el-table-column prop="environment" label="管道环境" width="110" />
        <el-table-column prop="operator" label="操作人" width="120" />
        <el-table-column prop="result" label="分析结果" min-width="120" />
        <el-table-column prop="createdAt" label="创建日期" width="120" />
        <el-table-column label="操作" width="110">
          <template #default>
            <el-button link type="success">详情</el-button>
            <el-button link type="success">导出</el-button>
          </template>
        </el-table-column>
      </el-table>
    </div>
  </section>
</template>

<script setup>
import { reactive } from 'vue'

defineProps({
  list: {
    type: Array,
    default: () => []
  },
  loading: {
    type: Boolean,
    default: false
  }
})

const emit = defineEmits(['search'])

const filters = reactive({
  mode: '',
  environment: '',
  startDate: '',
  endDate: ''
})

function handleSearch() {
  emit('search', { ...filters })
}
</script>

<style scoped>
.history-panel {
  min-height: 360px;
}
.history-body {
  padding: 12px 14px 16px;
}
.filter-form {
  margin-bottom: 10px;
}
:deep(.el-form-item) {
  margin-bottom: 10px;
}
:deep(.el-radio) {
  color: var(--text-muted);
}
</style>
