<template>
  <section class="panel analysis-panel">
    <div class="panel-title">监测分析</div>
    <div class="panel-body analysis-body">
      <div class="analysis-card">
        <div class="hexagon cyan">裂</div>
        <div>
          <h3>裂缝识别</h3>
          <p>下水管道结构完整，未见裂缝迹象，运行状态正常。</p>
        </div>
      </div>
      <div class="analysis-card">
        <div class="hexagon orange">变</div>
        <div>
          <h3 class="danger">变形分析</h3>
          <p>下水管道存在变形迹象，请及时安排复核和处理。</p>
        </div>
      </div>
    </div>
  </section>
</template>

<style scoped>
.analysis-panel {
  min-height: 420px;
  height: 100%;
}

.analysis-body {
  display: grid;
  gap: 18px;
  padding: 18px;
  align-content: start;
}

.analysis-card {
  display: grid;
  grid-template-columns: 88px 1fr;
  gap: 16px;
  align-items: center;
  min-height: 88px;
  padding: 14px 12px;
  background: rgba(22, 34, 49, 0.72);
}

.hexagon {
  display: grid;
  place-items: center;
  width: 72px;
  height: 72px;
  clip-path: polygon(25% 6%, 75% 6%, 100% 50%, 75% 94%, 25% 94%, 0 50%);
  font-size: 26px;
  font-weight: 700;
}

.cyan {
  color: #061720;
  background: linear-gradient(135deg, #74f0c2, #54e6ff);
  box-shadow: 0 0 18px rgba(84, 230, 255, 0.25);
}

.orange {
  color: #290d04;
  background: linear-gradient(135deg, #ff8a5e, #ff4c1e);
  box-shadow: 0 0 18px rgba(255, 110, 69, 0.2);
}

h3 {
  margin: 0 0 6px;
  color: #7ff3c8;
  font-size: 18px;
  line-height: 1.2;
}

.danger {
  color: #ff6b45;
}

p {
  margin: 0;
  color: var(--text-muted);
  font-size: 13px;
  line-height: 1.6;
}
</style>
