<template>
  <section class="panel history-panel">
    <div class="panel-title">历史查询</div>
    <div class="panel-body history-body">
      <el-form :inline="true" class="filter-form">
        <el-form-item>
          <el-radio-group v-model="filters.mode">
            <el-radio value="">全部</el-radio>
            <el-radio value="1">自动巡检</el-radio>
            <el-radio value="0">人工巡检</el-radio>
            <el-radio value="2">测试模式</el-radio>
          </el-radio-group>
        </el-form-item>
        <el-form-item>
          <el-date-picker
            v-model="filters.startDate"
            class="date-picker"
            type="date"
            placeholder="开始日期"
            value-format="YYYY-MM-DD"
          />
        </el-form-item>
        <el-form-item>
          <el-date-picker
            v-model="filters.endDate"
            class="date-picker"
            type="date"
            placeholder="结束日期"
            value-format="YYYY-MM-DD"
          />
        </el-form-item>
        <el-form-item>
          <el-button type="success" @click="handleSearch">查询</el-button>
        </el-form-item>
      </el-form>

      <div class="table-shell">
        <el-table :data="list" class="history-table" height="100%" v-loading="loading">
          <el-table-column type="index" label="序号" width="44" />
          <el-table-column prop="mode" label="巡检方式" min-width="74" show-overflow-tooltip />
          <el-table-column prop="environment" label="管道环境" min-width="84" show-overflow-tooltip />
          <el-table-column prop="operator" label="操作人" min-width="68" show-overflow-tooltip />
          <el-table-column prop="result" label="巡检结果" min-width="76" show-overflow-tooltip />
          <el-table-column prop="createdAt" label="创建日期" min-width="76" show-overflow-tooltip />
          <el-table-column label="操作" width="82">
            <template #default="{ row }">
              <div class="action-group">
                <el-button link type="success" @click="$emit('detail', row.id)">详情</el-button>
                <el-button link type="success" :loading="exportingId === row.id" @click="$emit('export', row.id)">导出</el-button>
              </div>
            </template>
          </el-table-column>
        </el-table>
      </div>

      <div class="pagination-shell">
        <el-pagination
          layout="prev, pager, next"
          :current-page="page"
          :page-size="pageSize"
          :total="total"
          @current-change="handlePageChange"
        />
      </div>
    </div>
  </section>
</template>

<script setup>
import { reactive, watch } from 'vue'

const props = defineProps({
  list: {
    type: Array,
    default: () => []
  },
  loading: {
    type: Boolean,
    default: false
  },
  exportingId: {
    type: Number,
    default: null
  },
  page: {
    type: Number,
    default: 1
  },
  pageSize: {
    type: Number,
    default: 5
  },
  total: {
    type: Number,
    default: 0
  },
  initialFilters: {
    type: Object,
    default: () => ({
      mode: '',
      environment: '',
      startDate: '',
      endDate: ''
    })
  }
})

const emit = defineEmits(['search', 'detail', 'export', 'page-change'])

const filters = reactive({
  mode: '',
  environment: '',
  startDate: '',
  endDate: ''
})

watch(
  () => props.initialFilters,
  (value) => {
    Object.assign(filters, value || {})
  },
  { immediate: true, deep: true }
)

function handleSearch() {
  emit('search', { ...filters })
}

function handlePageChange(page) {
  emit('page-change', page)
}
</script>

<style scoped>
.history-panel {
  min-height: 420px;
  height: 100%;
}

.history-body {
  padding: 10px 12px 12px;
  display: grid;
  grid-template-rows: auto minmax(0, 1fr) auto;
  gap: 10px;
  min-height: 0;
}

.filter-form {
  display: flex;
  flex-wrap: wrap;
  justify-content: center;
  align-items: center;
  gap: 8px 10px;
  padding: 2px 0;
}

.table-shell {
  min-height: 0;
}

.history-table {
  width: 100%;
  height: 100%;
  table-layout: fixed;
}

.pagination-shell {
  display: flex;
  justify-content: center;
  padding-top: 2px;
}

.action-group {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  white-space: nowrap;
}

:deep(.el-table .cell) {
  padding-left: 4px;
  padding-right: 4px;
}

:deep(.el-form-item) {
  margin-bottom: 0;
}

:deep(.el-form--inline .el-form-item) {
  margin-right: 0;
}

:deep(.date-picker) {
  width: 120px;
}

:deep(.el-radio),
:deep(.el-input__inner),
:deep(.el-input__wrapper),
:deep(.el-button),
:deep(.el-date-editor) {
  font-size: 13px;
}

:deep(.el-table th.el-table__cell) {
  font-size: 13px;
  font-weight: 600;
}

:deep(.el-table td.el-table__cell) {
  font-size: 13px;
}

:deep(.el-radio) {
  color: var(--text-dim);
  margin-right: 15px;
}

:deep(.el-pagination) {
  --el-pagination-button-color: #8af6cb;
  --el-pagination-text-color: #8af6cb;
  --el-pagination-hover-color: #8af6cb;
  color: #8af6cb;
}

:deep(.el-pagination .btn-prev),
:deep(.el-pagination .btn-next),
:deep(.el-pagination .el-pager li) {
  background: transparent;
  color: #8af6cb;
}

:deep(.el-pagination .el-pager li.is-active) {
  color: #8af6cb;
  border: 1px solid rgba(138, 246, 203, 0.45);
}
</style>
