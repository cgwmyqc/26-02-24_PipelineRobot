package com.hitzri.pipelinerobot.service;

import com.baomidou.mybatisplus.core.conditions.query.LambdaQueryWrapper;
import com.hitzri.pipelinerobot.entity.SysUser;
import com.hitzri.pipelinerobot.mapper.SysUserMapper;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.LocalDate;
import java.time.LocalDateTime;
import org.springframework.boot.CommandLineRunner;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.stereotype.Component;

@Component
public class DataSeeder implements CommandLineRunner {

    private final SysUserMapper sysUserMapper;
    private final PasswordEncoder passwordEncoder;
    private final InspectionService inspectionService;

    public DataSeeder(
        SysUserMapper sysUserMapper,
        PasswordEncoder passwordEncoder,
        InspectionService inspectionService
    ) {
        this.sysUserMapper = sysUserMapper;
        this.passwordEncoder = passwordEncoder;
        this.inspectionService = inspectionService;
    }

    @Override
    public void run(String... args) throws Exception {
        seedUser();
        seedPlaceholderMedia();
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
        user.setDisplayName("System Admin");
        user.setStatus("ACTIVE");
        user.setRole("ADMIN");
        user.setCreatedAt(LocalDateTime.now());
        sysUserMapper.insert(user);
    }

    private void seedPlaceholderMedia() throws IOException {
        LocalDate startDate = LocalDate.of(2026, 3, 20);
        for (int index = 1; index <= 10; index++) {
            LocalDate currentDate = startDate.minusDays(index - 1L);
            String ts = String.format("%s%02d", currentDate.toString().replace("-", ""), index);
            writePlaceholder(
                String.format("results/%s/videos/inspection-%s-%02d.mp4", ts, currentDate, index),
                "demo video placeholder " + index
            );
        }

        for (int index = 1; index <= 6; index++) {
            writePlaceholder(
                String.format("results/20260320%02d/images/anomaly-202603-%02d.jpg", index, index),
                "demo image placeholder " + index
            );
        }
    }

    private void writePlaceholder(String relativePath, String content) throws IOException {
        Path path = inspectionService.resolveStoragePath(relativePath);
        Files.createDirectories(path.getParent());
        if (!Files.exists(path)) {
            Files.writeString(path, content, StandardCharsets.UTF_8);
        }
    }
}
