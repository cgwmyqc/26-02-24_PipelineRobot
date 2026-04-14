package com.hitzri.pipelinerobot.service;

import com.baomidou.mybatisplus.core.conditions.query.LambdaQueryWrapper;
import com.hitzri.pipelinerobot.entity.InspectionAnomalyImage;
import com.hitzri.pipelinerobot.entity.InspectionPointFile;
import com.hitzri.pipelinerobot.entity.InspectionRecord;
import com.hitzri.pipelinerobot.entity.SysUser;
import com.hitzri.pipelinerobot.mapper.InspectionAnomalyImageMapper;
import com.hitzri.pipelinerobot.mapper.InspectionPointFileMapper;
import com.hitzri.pipelinerobot.mapper.InspectionRecordMapper;
import com.hitzri.pipelinerobot.mapper.SysUserMapper;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;
import java.util.Random;
import java.util.Set;
import org.springframework.core.io.Resource;
import org.springframework.core.io.support.PathMatchingResourcePatternResolver;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.CommandLineRunner;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.stereotype.Component;
import org.springframework.transaction.annotation.Transactional;

@Component
public class DataSeeder implements CommandLineRunner {

    private static final DateTimeFormatter RESULT_TS_FORMATTER = DateTimeFormatter.ofPattern("yyyyMMddHHmmss");
    private static final Random DEMO_RANDOM = new Random(20260414L);
    private static final Set<String> ANOMALY_CODES = Set.of("PL", "BX", "SG", "ZAW", "RG");
    private static final List<String> LEGACY_DEMO_RESULT_DIRS = List.of(
        "results/2026031110",
        "results/2026031209",
        "results/2026031308",
        "results/2026031407",
        "results/2026031506",
        "results/2026031605",
        "results/2026031704",
        "results/2026031803",
        "results/2026031902",
        "results/2026032001",
        "results/2026032002",
        "results/2026032003",
        "results/2026032004",
        "results/2026032005",
        "results/2026032006"
    );

    private final SysUserMapper sysUserMapper;
    private final PasswordEncoder passwordEncoder;
    private final InspectionRecordMapper inspectionRecordMapper;
    private final InspectionAnomalyImageMapper inspectionAnomalyImageMapper;
    private final InspectionPointFileMapper inspectionPointFileMapper;
    private final InspectionService inspectionService;
    private final boolean seedEnabled;

    public DataSeeder(
        SysUserMapper sysUserMapper,
        PasswordEncoder passwordEncoder,
        InspectionRecordMapper inspectionRecordMapper,
        InspectionAnomalyImageMapper inspectionAnomalyImageMapper,
        InspectionPointFileMapper inspectionPointFileMapper,
        InspectionService inspectionService,
        @Value("${app.demo.seed-enabled:true}") boolean seedEnabled
    ) {
        this.sysUserMapper = sysUserMapper;
        this.passwordEncoder = passwordEncoder;
        this.inspectionRecordMapper = inspectionRecordMapper;
        this.inspectionAnomalyImageMapper = inspectionAnomalyImageMapper;
        this.inspectionPointFileMapper = inspectionPointFileMapper;
        this.inspectionService = inspectionService;
        this.seedEnabled = seedEnabled;
    }

    @Override
    public void run(String... args) throws Exception {
        seedUser();
        seedDemoInspectionData();
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

    @Transactional
    protected void seedDemoInspectionData() throws IOException {
        if (!seedEnabled || inspectionRecordMapper.selectCount(null) > 0) {
            return;
        }

        clearLegacyDemoDirectories();
        DemoSeedAssets demoSeedAssets = loadDemoSeedAssets();

        for (DemoInspectionSeed seed : buildDemoSeeds(demoSeedAssets)) {
            InspectionRecord record = new InspectionRecord();
            record.setMode(seed.mode());
            record.setEnvironment(seed.environment());
            record.setOperatorName("admin");
            record.setResultSummary(seed.resultSummary());
            record.setResultDir(seed.resultDir());
            record.setVideoPath(seed.videoPath());
            record.setFittedResultPath(seed.fittedResultPath());
            record.setInspectionTime(seed.inspectionTime());
            record.setCreatedAt(seed.createdAt());
            inspectionRecordMapper.insert(record);

            writeBinaryPlaceholder(seed.videoPath(), demoSeedAssets.video().bytes());

            for (DemoAnomalySeed anomaly : seed.anomalies()) {
                InspectionAnomalyImage image = new InspectionAnomalyImage();
                image.setInspectionId(record.getId());
                image.setImagePath(anomaly.path());
                image.setAnomalyType(anomaly.type());
                image.setRemark(anomaly.fileName());
                image.setCapturedAt(anomaly.capturedAt());
                inspectionAnomalyImageMapper.insert(image);
                writeBinaryPlaceholder(anomaly.path(), anomaly.seedResource().bytes());
            }

            for (DemoPointSeed point : seed.points()) {
                InspectionPointFile pointFile = new InspectionPointFile();
                pointFile.setInspectionId(record.getId());
                pointFile.setPointPath(point.path());
                pointFile.setCapturedAt(point.capturedAt());
                inspectionPointFileMapper.insert(pointFile);
                writeBinaryPlaceholder(point.path(), demoSeedAssets.point().bytes());
            }

            writePlaceholder(seed.fittedResultPath(), buildFittedResultJson(seed.recordNumber()));
        }
    }

    private void clearLegacyDemoDirectories() throws IOException {
        for (String relativeDir : LEGACY_DEMO_RESULT_DIRS) {
            deleteDirectoryIfExists(relativeDir);
        }
    }

    private void writePlaceholder(String relativePath, String content) throws IOException {
        Path path = inspectionService.resolveStoragePath(relativePath);
        Files.createDirectories(path.getParent());
        if (!Files.exists(path)) {
            Files.writeString(path, content, StandardCharsets.UTF_8);
        }
    }

    private void writeBinaryPlaceholder(String relativePath, byte[] content) throws IOException {
        Path path = inspectionService.resolveStoragePath(relativePath);
        Files.createDirectories(path.getParent());
        if (!Files.exists(path)) {
            Files.write(path, content);
        }
    }

    private DemoSeedAssets loadDemoSeedAssets() {
        PathMatchingResourcePatternResolver resolver = new PathMatchingResourcePatternResolver();
        List<SeedResource> images = loadSeedResources(resolver, "classpath:database_seed/images/*");
        List<SeedResource> videos = loadSeedResources(resolver, "classpath:database_seed/videos/*");
        List<SeedResource> points = loadSeedResources(resolver, "classpath:database_seed/points/cloud_accum.pcd");

        if (images.isEmpty()) {
            throw new IllegalStateException("database_seed/images 缺少异常图片素材，无法生成默认模拟数据");
        }
        if (videos.isEmpty()) {
            throw new IllegalStateException("database_seed/videos 缺少视频素材，无法生成默认模拟数据");
        }
        if (points.isEmpty()) {
            throw new IllegalStateException("database_seed/points 缺少点云素材，无法生成默认模拟数据");
        }

        return new DemoSeedAssets(
            images,
            videos.get(0),
            points.get(0)
        );
    }

    private List<SeedResource> loadSeedResources(PathMatchingResourcePatternResolver resolver, String pattern) {
        try {
            Resource[] resources = resolver.getResources(pattern);
            List<SeedResource> seedResources = new ArrayList<>();
            for (Resource resource : resources) {
                if (!resource.exists() || !resource.isReadable()) {
                    continue;
                }
                String filename = resource.getFilename();
                if (filename == null || filename.isBlank()) {
                    continue;
                }
                try (InputStream inputStream = resource.getInputStream()) {
                    seedResources.add(new SeedResource(filename, inputStream.readAllBytes()));
                }
            }
            seedResources.sort(Comparator.comparing(SeedResource::fileName));
            return List.copyOf(seedResources);
        } catch (IOException error) {
            return List.of();
        }
    }

    private void deleteDirectoryIfExists(String relativePath) throws IOException {
        Path path = inspectionService.resolveStoragePath(relativePath);
        if (!Files.exists(path)) {
            return;
        }
        try (java.util.stream.Stream<Path> stream = Files.walk(path)) {
            stream.sorted(java.util.Comparator.reverseOrder())
                .forEach(current -> {
                    try {
                        Files.deleteIfExists(current);
                    } catch (IOException error) {
                        throw new RuntimeException(error);
                    }
                });
        } catch (RuntimeException error) {
            if (error.getCause() instanceof IOException ioException) {
                throw ioException;
            }
            throw error;
        }
    }

    private String buildFittedResultJson(int recordNumber) {
        double radius = 0.46 + DEMO_RANDOM.nextDouble() * 0.08;
        double startZ = -1.6 + recordNumber * 0.05;
        double pipeLength = 2.4 + DEMO_RANDOM.nextDouble() * 1.0;
        double endZ = startZ + pipeLength;

        String pointDefect = """
            {
              "class_name": "%s",
              "z_m": %.3f,
              "theta_rad": %.3f
            }
            """.formatted(
            pickRandomDefectCode(false),
            normalizeRange(startZ + 0.35 + DEMO_RANDOM.nextDouble() * 0.55, startZ, endZ),
            DEMO_RANDOM.nextDouble() * Math.PI * 2
        ).trim();

        double patchCenter = normalizeRange(startZ + 1.2 + DEMO_RANDOM.nextDouble() * 0.7, startZ + 0.45, endZ - 0.25);
        double patchHalfSpan = 0.12 + DEMO_RANDOM.nextDouble() * 0.18;
        String patchDefect = """
            {
              "class_name": "%s",
              "region_type": "patch",
              "z_m": %.3f,
              "z_min": %.3f,
              "z_max": %.3f,
              "theta_center": %.3f,
              "theta_min_offset": %.3f,
              "theta_max_offset": %.3f
            }
            """.formatted(
            pickRandomDefectCode(true),
            patchCenter,
            normalizeRange(patchCenter - patchHalfSpan, startZ, endZ),
            normalizeRange(patchCenter + patchHalfSpan, startZ, endZ),
            DEMO_RANDOM.nextDouble() * Math.PI * 2,
            -0.10 - DEMO_RANDOM.nextDouble() * 0.16,
            0.10 + DEMO_RANDOM.nextDouble() * 0.16
        ).trim();

        String maybeThirdDefect = "";
        if (DEMO_RANDOM.nextBoolean()) {
            maybeThirdDefect = """
                ,
                {
                  "class_name": "%s",
                  "z_m": %.3f,
                  "theta_rad": %.3f
                }
                """.formatted(
                pickRandomDefectCode(false),
                normalizeRange(startZ + 0.6 + DEMO_RANDOM.nextDouble() * 1.4, startZ, endZ),
                DEMO_RANDOM.nextDouble() * Math.PI * 2
            );
        }

        return """
            {
              "R_global_m": %.3f,
              "z_range_m": [%.3f, %.3f],
              "pipe_length_m": %.3f,
              "defects": [
                %s,
                %s%s
              ]
            }
            """.formatted(radius, startZ, endZ, pipeLength, pointDefect, patchDefect, maybeThirdDefect).trim();
    }

    private String pickRandomDefectCode(boolean patchPreferred) {
        List<String> candidates = patchPreferred ? List.of("BX", "SG", "ZAW") : List.of("PL", "RG", "BX", "SG");
        return candidates.get(DEMO_RANDOM.nextInt(candidates.size()));
    }

    private double normalizeRange(double value, double min, double max) {
        return Math.max(min, Math.min(max, value));
    }

    private List<DemoInspectionSeed> buildDemoSeeds(DemoSeedAssets assets) {
        return List.of(
            createDemoSeed(1, "1", "0", "1", 2, assets),
            createDemoSeed(2, "0", "1", "2", 1, assets),
            createDemoSeed(3, "2", "1", "2", 2, assets),
            createDemoSeed(4, "1", "1", "0", 1, assets),
            createDemoSeed(5, "0", "0", "1", 2, assets),
            createDemoSeed(6, "2", "0", "2", 1, assets),
            createDemoSeed(7, "1", "1", "2", 2, assets),
            createDemoSeed(8, "0", "1", "0", 1, assets),
            createDemoSeed(9, "2", "0", "1", 2, assets),
            createDemoSeed(10, "1", "1", "0", 1, assets)
        );
    }

    private DemoInspectionSeed createDemoSeed(
        int recordNumber,
        String mode,
        String environment,
        String resultSummary,
        int anomalyCount,
        DemoSeedAssets assets
    ) {
        LocalDateTime createdAt = LocalDateTime.of(2026, 3, 21 - recordNumber, 9 + (recordNumber % 3), 10 + recordNumber, 0);
        LocalDateTime inspectionTime = createdAt.minusMinutes(3 + (recordNumber % 4));
        String ts = createdAt.format(RESULT_TS_FORMATTER);
        String resultDir = "results/" + ts;
        String videoPath = resultDir + "/videos/inspection_" + mode + "_" + ts + ".mp4";

        List<DemoAnomalySeed> anomalies = buildAnomalySeeds(resultDir, createdAt, anomalyCount, assets);
        List<DemoPointSeed> points = buildPointSeeds(resultDir, createdAt);
        String fittedResultPath = resultDir + "/points/defects_global.json";

        return new DemoInspectionSeed(
            recordNumber,
            mode,
            environment,
            resultSummary,
            inspectionTime,
            createdAt,
            resultDir,
            videoPath,
            fittedResultPath,
            anomalies,
            points
        );
    }

    private List<DemoAnomalySeed> buildAnomalySeeds(
        String resultDir,
        LocalDateTime createdAt,
        int anomalyCount,
        DemoSeedAssets assets
    ) {
        List<SeedResource> shuffledImages = new ArrayList<>(assets.images());
        Collections.shuffle(shuffledImages, DEMO_RANDOM);
        return java.util.stream.IntStream.range(0, anomalyCount)
            .mapToObj(index -> {
                SeedResource imageResource = shuffledImages.get(index % shuffledImages.size());
                String sourceFileName = imageResource.fileName();
                String fileName = "image_%02d_%s".formatted(index + 1, sourceFileName);
                return new DemoAnomalySeed(
                    resultDir + "/images/" + fileName,
                    parseAnomalyTypeFromFileName(sourceFileName),
                    fileName,
                    imageResource,
                    createdAt.minusMinutes(Math.max(1, anomalyCount - index))
                );
            }).toList();
    }

    private List<DemoPointSeed> buildPointSeeds(String resultDir, LocalDateTime createdAt) {
        return List.of(new DemoPointSeed(
            resultDir + "/points/assembled_cloud_01.pcd",
            createdAt.minusSeconds(15)
        ));
    }

    private record DemoInspectionSeed(
        int recordNumber,
        String mode,
        String environment,
        String resultSummary,
        LocalDateTime inspectionTime,
        LocalDateTime createdAt,
        String resultDir,
        String videoPath,
        String fittedResultPath,
        List<DemoAnomalySeed> anomalies,
        List<DemoPointSeed> points
    ) {}

    private record DemoAnomalySeed(
        String path,
        String type,
        String fileName,
        SeedResource seedResource,
        LocalDateTime capturedAt
    ) {}

    private record DemoPointSeed(
        String path,
        LocalDateTime capturedAt
    ) {}

    private String parseAnomalyTypeFromFileName(String fileName) {
        String baseName = fileName.replaceFirst("\\.[^.]+$", "");
        List<String> matchedTypes = Arrays.stream(baseName.split("_"))
            .map(part -> part.trim().toUpperCase(Locale.ROOT))
            .filter(ANOMALY_CODES::contains)
            .distinct()
            .toList();
        if (matchedTypes.isEmpty()) {
            return "RG";
        }
        return String.join("+", matchedTypes);
    }

    private record SeedResource(
        String fileName,
        byte[] bytes
    ) {}

    private record DemoSeedAssets(
        List<SeedResource> images,
        SeedResource video,
        SeedResource point
    ) {}
}
