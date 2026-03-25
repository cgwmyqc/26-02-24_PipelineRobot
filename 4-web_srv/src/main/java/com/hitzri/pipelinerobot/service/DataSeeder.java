package com.hitzri.pipelinerobot.service;

import com.baomidou.mybatisplus.core.conditions.query.LambdaQueryWrapper;
import com.hitzri.pipelinerobot.entity.InspectionAnomalyImage;
import com.hitzri.pipelinerobot.entity.InspectionRecord;
import com.hitzri.pipelinerobot.entity.SysUser;
import com.hitzri.pipelinerobot.mapper.InspectionAnomalyImageMapper;
import com.hitzri.pipelinerobot.mapper.InspectionRecordMapper;
import com.hitzri.pipelinerobot.mapper.SysUserMapper;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.LocalDateTime;
import org.springframework.boot.CommandLineRunner;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.stereotype.Component;

@Component
public class DataSeeder implements CommandLineRunner {

    private final SysUserMapper sysUserMapper;
    private final InspectionRecordMapper inspectionRecordMapper;
    private final InspectionAnomalyImageMapper anomalyImageMapper;
    private final PasswordEncoder passwordEncoder;
    private final InspectionService inspectionService;

    public DataSeeder(
        SysUserMapper sysUserMapper,
        InspectionRecordMapper inspectionRecordMapper,
        InspectionAnomalyImageMapper anomalyImageMapper,
        PasswordEncoder passwordEncoder,
        InspectionService inspectionService
    ) {
        this.sysUserMapper = sysUserMapper;
        this.inspectionRecordMapper = inspectionRecordMapper;
        this.anomalyImageMapper = anomalyImageMapper;
        this.passwordEncoder = passwordEncoder;
        this.inspectionService = inspectionService;
    }

    @Override
    public void run(String... args) throws Exception {
        seedUser();
        seedInspectionData();
    }

    private void seedUser() {
        LambdaQueryWrapper<SysUser> wrapper = new LambdaQueryWrapper<>();
        wrapper.eq(SysUser::getUsername, "admin").last("LIMIT 1");
        if (sysUserMapper.selectOne(wrapper) != null) {
            return;
        }

        SysUser user = new SysUser();
        user.setUsername("admin");
        user.setPasswordHash(passwordEncoder.encode("admin123"));
        user.setDisplayName("系统管理员");
        user.setStatus("ACTIVE");
        user.setRole("ADMIN");
        user.setCreatedAt(LocalDateTime.now());
        sysUserMapper.insert(user);
    }

    private void seedInspectionData() throws IOException {
        if (inspectionRecordMapper.selectCount(null) > 0) {
            return;
        }

        writePlaceholder("videos/inspection-20260320.mp4", "demo video placeholder");
        writePlaceholder("images/anomaly-20260320-1.jpg", "demo image placeholder");

        InspectionRecord record = new InspectionRecord();
        record.setMode("自动巡检");
        record.setEnvironment("满水环境");
        record.setOperatorName("系统管理员");
        record.setResultSummary("裂缝预警");
        record.setVideoPath("videos/inspection-20260320.mp4");
        record.setInspectionTime(LocalDateTime.of(2026, 3, 20, 10, 28));
        record.setCreatedAt(LocalDateTime.of(2026, 3, 20, 10, 30));
        inspectionRecordMapper.insert(record);

        InspectionAnomalyImage image = new InspectionAnomalyImage();
        image.setInspectionId(record.getId());
        image.setImagePath("images/anomaly-20260320-1.jpg");
        image.setAnomalyType("裂缝");
        image.setRemark("管壁中段发现疑似裂缝");
        image.setCapturedAt(LocalDateTime.of(2026, 3, 20, 10, 29));
        anomalyImageMapper.insert(image);

        InspectionRecord cleanRecord = new InspectionRecord();
        cleanRecord.setMode("人工巡检");
        cleanRecord.setEnvironment("非满水环境");
        cleanRecord.setOperatorName("系统管理员");
        cleanRecord.setResultSummary("未发现异常");
        cleanRecord.setVideoPath("videos/inspection-20260320.mp4");
        cleanRecord.setInspectionTime(LocalDateTime.of(2026, 3, 18, 14, 18));
        cleanRecord.setCreatedAt(LocalDateTime.of(2026, 3, 18, 14, 20));
        inspectionRecordMapper.insert(cleanRecord);
    }

    private void writePlaceholder(String relativePath, String content) throws IOException {
        Path path = inspectionService.resolveStoragePath(relativePath);
        Files.createDirectories(path.getParent());
        if (!Files.exists(path)) {
            Files.writeString(path, content, StandardCharsets.UTF_8);
        }
    }
}
