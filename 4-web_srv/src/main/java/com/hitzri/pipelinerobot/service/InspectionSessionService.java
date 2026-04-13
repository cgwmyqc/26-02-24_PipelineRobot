package com.hitzri.pipelinerobot.service;

import com.hitzri.pipelinerobot.config.StorageProperties;
import com.hitzri.pipelinerobot.entity.InspectionAnomalyImage;
import com.hitzri.pipelinerobot.entity.InspectionPointFile;
import com.hitzri.pipelinerobot.entity.InspectionRecord;
import com.hitzri.pipelinerobot.mapper.InspectionAnomalyImageMapper;
import com.hitzri.pipelinerobot.mapper.InspectionPointFileMapper;
import com.hitzri.pipelinerobot.mapper.InspectionRecordMapper;
import com.hitzri.pipelinerobot.vo.InspectionMediaFileVO;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.TimeUnit;
import java.util.stream.Stream;
import jakarta.annotation.PreDestroy;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.util.StringUtils;

@Service
public class InspectionSessionService {

    private static final Logger log = LoggerFactory.getLogger(InspectionSessionService.class);
    private static final DateTimeFormatter RESULT_TS_FORMATTER = DateTimeFormatter.ofPattern("yyyyMMddHHmmss");
    private static final Set<String> SUPPORTED_ANOMALY_TYPES = Set.of("PL", "BX", "SG", "ZAW", "RG");

    private final InspectionRecordMapper inspectionRecordMapper;
    private final InspectionAnomalyImageMapper anomalyImageMapper;
    private final InspectionPointFileMapper pointFileMapper;
    private final StorageProperties storageProperties;

    private final Map<String, SessionContext> sessions = new ConcurrentHashMap<>();

    public InspectionSessionService(
        InspectionRecordMapper inspectionRecordMapper,
        InspectionAnomalyImageMapper anomalyImageMapper,
        InspectionPointFileMapper pointFileMapper,
        StorageProperties storageProperties
    ) {
        this.inspectionRecordMapper = inspectionRecordMapper;
        this.anomalyImageMapper = anomalyImageMapper;
        this.pointFileMapper = pointFileMapper;
        this.storageProperties = storageProperties;
    }

    @Transactional
    public String startSession(String mode, String environment, String operatorName) throws IOException {
        validateMode(mode);
        String normalizedEnvironment = StringUtils.hasText(environment) ? environment.trim() : "0";
        String normalizedOperator = StringUtils.hasText(operatorName) ? operatorName.trim() : "system";

        String sessionId = "sess_" + UUID.randomUUID().toString().replace("-", "");
        Path sessionDir = resolveStoragePath(getSessionsDir()).resolve(sessionId).normalize();
        Path captureDir = sessionDir.resolve("images");
        Path tempVideoPath = sessionDir.resolve("recording.mp4");
        Path ffmpegLogPath = sessionDir.resolve("ffmpeg.log");

        Files.createDirectories(captureDir);
        Process recorderProcess = startRecorderProcess(tempVideoPath, ffmpegLogPath);

        SessionContext context = new SessionContext();
        context.sessionId = sessionId;
        context.mode = mode.trim();
        context.environment = normalizedEnvironment;
        context.operatorName = normalizedOperator;
        context.startedAt = LocalDateTime.now();
        context.sessionDir = sessionDir;
        context.captureDir = captureDir;
        context.tempVideoPath = tempVideoPath;
        context.recorderProcess = recorderProcess;
        sessions.put(sessionId, context);

        return sessionId;
    }

    public InspectionMediaFileVO captureFrame(String sessionId) throws IOException, InterruptedException {
        SessionContext context = getSession(sessionId);
        if (!"0".equals(context.mode)) {
            throw new IllegalStateException("仅手动模式支持抓拍");
        }
        ensureRecorderRunning(context);

        context.captureSequence += 1;
        String fileName = String.format("stop_%04d_RG.jpg", context.captureSequence);
        Path targetPath = context.captureDir.resolve(fileName).normalize();

        List<String> command = buildSnapshotCommand(targetPath);
        Process process = new ProcessBuilder(command)
            .redirectErrorStream(true)
            .start();

        boolean finished = process.waitFor(15, TimeUnit.SECONDS);
        if (!finished) {
            process.destroyForcibly();
            throw new IllegalStateException("抓拍超时");
        }
        if (process.exitValue() != 0 || !Files.exists(targetPath)) {
            throw new IllegalStateException("抓拍失败");
        }

        InspectionMediaFileVO vo = new InspectionMediaFileVO();
        vo.setFileName(fileName);
        vo.setFilePath(fileName);
        vo.setCapturedAt(LocalDateTime.now());
        return vo;
    }

    @Transactional
    public Long finishSession(
        String sessionId,
        boolean copyDefectImages,
        String pointCloudFileName,
        String pointCloudPcdContent
    ) throws IOException, InterruptedException {
        SessionContext context = getSession(sessionId);
        stopRecorderProcess(context);

        LocalDateTime now = LocalDateTime.now();
        String ts = now.format(RESULT_TS_FORMATTER);
        String resultDir = String.format("%s/%s", getResultsDir(), ts);

        Path resultBase = resolveStoragePath(resultDir);
        Path videosDir = resultBase.resolve(getVideoDir());
        Path imagesDir = resultBase.resolve(getImageDir());
        Path pointsDir = resultBase.resolve(getPointsDir());
        Files.createDirectories(videosDir);
        Files.createDirectories(imagesDir);
        Files.createDirectories(pointsDir);

        if (!Files.exists(context.tempVideoPath)) {
            throw new IllegalStateException("录像文件不存在，无法完成巡检");
        }

        String videoFileName = String.format("inspection_%s_%s.mp4", context.mode, ts);
        Path finalVideoPath = videosDir.resolve(videoFileName).normalize();
        Files.move(context.tempVideoPath, finalVideoPath, StandardCopyOption.REPLACE_EXISTING);

        String relativeVideoPath = toRelativeStoragePath(finalVideoPath);

        InspectionRecord record = new InspectionRecord();
        record.setMode(context.mode);
        record.setEnvironment(context.environment);
        record.setOperatorName(context.operatorName);
        record.setResultSummary("0");
        record.setResultDir(resultDir);
        record.setVideoPath(relativeVideoPath);
        record.setInspectionTime(context.startedAt);
        record.setCreatedAt(now);
        inspectionRecordMapper.insert(record);

        List<String> anomalyTypes = new ArrayList<>();

        List<Path> captureFiles = listFiles(context.captureDir);
        for (Path captureFile : captureFiles) {
            Path target = imagesDir.resolve(captureFile.getFileName().toString()).normalize();
            Files.move(captureFile, target, StandardCopyOption.REPLACE_EXISTING);
            String relativePath = toRelativeStoragePath(target);
            String anomalyType = "RG";
            anomalyTypes.add(anomalyType);
            insertAnomalyImage(record.getId(), relativePath, anomalyType, captureFile.getFileName().toString(), now);
        }

        if (copyDefectImages) {
            for (Path copiedPath : copyDefectImagesToResult(imagesDir)) {
                String fileName = copiedPath.getFileName().toString();
                String anomalyType = parseAnomalyType(fileName);
                anomalyTypes.add(anomalyType);
                insertAnomalyImage(record.getId(), toRelativeStoragePath(copiedPath), anomalyType, fileName, now);
            }
        }

        savePointCloudFile(record.getId(), pointsDir, pointCloudFileName, pointCloudPcdContent, now);

        record.setResultSummary(calculateResultSummary(anomalyTypes));
        inspectionRecordMapper.updateById(record);

        sessions.remove(sessionId);
        deleteRecursively(context.sessionDir);
        return record.getId();
    }

    public void abortSession(String sessionId) throws IOException {
        SessionContext context = sessions.remove(sessionId);
        if (context == null) {
            return;
        }
        stopRecorderProcessQuietly(context);
        deleteRecursively(context.sessionDir);
    }

    public boolean hasSession(String sessionId) {
        return sessions.containsKey(sessionId);
    }

    @PreDestroy
    public void shutdownAllSessions() {
        for (String sessionId : new ArrayList<>(sessions.keySet())) {
            try {
                abortSession(sessionId);
            } catch (Exception error) {
                log.warn("Failed to close inspection session {} during shutdown: {}", sessionId, error.getMessage());
            }
        }
    }

    private void insertAnomalyImage(Long inspectionId, String relativePath, String anomalyType, String remark, LocalDateTime capturedAt) {
        InspectionAnomalyImage image = new InspectionAnomalyImage();
        image.setInspectionId(inspectionId);
        image.setImagePath(relativePath);
        image.setAnomalyType(anomalyType);
        image.setRemark(remark);
        image.setCapturedAt(capturedAt);
        anomalyImageMapper.insert(image);
    }

    private void insertPointFile(Long inspectionId, String relativePath, LocalDateTime capturedAt) {
        InspectionPointFile pointFile = new InspectionPointFile();
        pointFile.setInspectionId(inspectionId);
        pointFile.setPointPath(relativePath);
        pointFile.setCapturedAt(capturedAt);
        pointFileMapper.insert(pointFile);
    }

    private void savePointCloudFile(
        Long inspectionId,
        Path pointsDir,
        String pointCloudFileName,
        String pointCloudPcdContent,
        LocalDateTime capturedAt
    ) throws IOException {
        String normalizedContent = String.valueOf(pointCloudPcdContent == null ? "" : pointCloudPcdContent).trim();
        if (!StringUtils.hasText(normalizedContent)) {
            return;
        }

        String normalizedFileName = StringUtils.hasText(pointCloudFileName)
            ? Paths.get(pointCloudFileName).getFileName().toString()
            : "pointclouds.pcd";
        if (!normalizedFileName.toLowerCase(Locale.ROOT).endsWith(".pcd")) {
            normalizedFileName = normalizedFileName + ".pcd";
        }

        Path targetPath = pointsDir.resolve(normalizedFileName).normalize();
        Files.writeString(targetPath, normalizedContent + System.lineSeparator(), StandardCharsets.UTF_8);
        insertPointFile(inspectionId, toRelativeStoragePath(targetPath), capturedAt);
    }

    private List<Path> copyDefectImagesToResult(Path imagesDir) throws IOException {
        Path sourceDir = Paths.get(storageProperties.getPipeDatasetRoot())
            .toAbsolutePath()
            .normalize()
            .resolve("defect_images_boxed")
            .normalize();

        if (!Files.exists(sourceDir) || !Files.isDirectory(sourceDir)) {
            return Collections.emptyList();
        }

        List<Path> copied = new ArrayList<>();
        for (Path sourceFile : listFiles(sourceDir)) {
            String fileName = sourceFile.getFileName().toString();
            Path target = imagesDir.resolve(fileName).normalize();
            Files.copy(sourceFile, target, StandardCopyOption.REPLACE_EXISTING);
            copied.add(target);
        }
        return copied;
    }

    private String calculateResultSummary(List<String> anomalyTypes) {
        if (anomalyTypes.isEmpty()) {
            return "0";
        }

        boolean hasCrackOrManual = anomalyTypes.stream()
            .anyMatch(type -> String.valueOf(type).contains("PL") || String.valueOf(type).contains("RG"));
        return hasCrackOrManual ? "1" : "2";
    }

    private String parseAnomalyType(String fileName) {
        String baseName = fileName;
        int dotIndex = fileName.lastIndexOf('.');
        if (dotIndex > 0) {
            baseName = fileName.substring(0, dotIndex);
        }

        Set<String> detected = new LinkedHashSet<>();
        for (String token : baseName.split("_")) {
            String upper = token.trim().toUpperCase(Locale.ROOT);
            if (SUPPORTED_ANOMALY_TYPES.contains(upper)) {
                detected.add(upper);
            }
        }

        return detected.isEmpty() ? "UNKNOWN" : String.join("+", detected);
    }

    private void ensureRecorderRunning(SessionContext context) {
        if (context.recorderProcess == null || !context.recorderProcess.isAlive()) {
            throw new IllegalStateException("录像进程未运行");
        }
    }

    private void stopRecorderProcess(SessionContext context) throws IOException, InterruptedException {
        if (context.recorderProcess == null || !context.recorderProcess.isAlive()) {
            return;
        }

        try (Writer writer = new OutputStreamWriter(context.recorderProcess.getOutputStream(), StandardCharsets.UTF_8)) {
            writer.write("q\n");
            writer.flush();
        } catch (IOException error) {
            log.debug("Failed to write quit signal to ffmpeg: {}", error.getMessage());
        }

        boolean exited = context.recorderProcess.waitFor(6, TimeUnit.SECONDS);
        if (!exited) {
            context.recorderProcess.destroy();
            exited = context.recorderProcess.waitFor(3, TimeUnit.SECONDS);
        }
        if (!exited) {
            context.recorderProcess.destroyForcibly();
            context.recorderProcess.waitFor(2, TimeUnit.SECONDS);
        }
    }

    private void stopRecorderProcessQuietly(SessionContext context) {
        try {
            stopRecorderProcess(context);
        } catch (Exception error) {
            log.warn("Failed to stop recorder process for session {}: {}", context.sessionId, error.getMessage());
            if (context.recorderProcess != null && context.recorderProcess.isAlive()) {
                context.recorderProcess.destroyForcibly();
            }
        }
    }

    private Process startRecorderProcess(Path outputVideoPath, Path logPath) throws IOException {
        List<String> command = buildRecordCommand(outputVideoPath);
        ProcessBuilder processBuilder = new ProcessBuilder(command)
            .redirectErrorStream(true)
            .redirectOutput(logPath.toFile());
        Process process;
        try {
            process = processBuilder.start();
        } catch (IOException exception) {
            throw new IllegalStateException(buildFfmpegStartErrorMessage(command.get(0), exception), exception);
        }

        if (!process.isAlive()) {
            throw new IllegalStateException("录像进程启动失败");
        }

        return process;
    }

    private List<String> buildRecordCommand(Path outputVideoPath) {
        List<String> command = new ArrayList<>();
        command.add(StringUtils.hasText(storageProperties.getFfmpegBin()) ? storageProperties.getFfmpegBin() : "ffmpeg");
        command.add("-y");

        String source = getCameraRtspUrl();
        if (source.startsWith("rtsp://")) {
            command.addAll(List.of("-rtsp_transport", "tcp"));
        }

        command.addAll(List.of("-i", source, "-c", "copy", outputVideoPath.toString()));
        return command;
    }

    private List<String> buildSnapshotCommand(Path targetPath) {
        List<String> command = new ArrayList<>();
        command.add(StringUtils.hasText(storageProperties.getFfmpegBin()) ? storageProperties.getFfmpegBin() : "ffmpeg");
        command.add("-y");

        String source = getCameraRtspUrl();
        if (source.startsWith("rtsp://")) {
            command.addAll(List.of("-rtsp_transport", "tcp"));
        }

        command.addAll(Arrays.asList("-i", source, "-frames:v", "1", targetPath.toString()));
        return command;
    }

    private String buildFfmpegStartErrorMessage(String executable, IOException exception) {
        String message = exception.getMessage();
        if (message != null && (message.contains("error=2") || message.contains("No such file") || message.contains("没有那个文件或目录"))) {
            return "无法启动FFmpeg，请检查 FFMPEG_BIN 配置或确认 ffmpeg 已安装";
        }
        return "启动录像进程失败: " + (message != null ? message : executable);
    }

    private String getCameraRtspUrl() {
        String source = String.valueOf(storageProperties.getCameraRtspUrl()).trim();
        if (!StringUtils.hasText(source)) {
            throw new IllegalStateException("未配置相机 RTSP 地址");
        }
        return source;
    }

    private SessionContext getSession(String sessionId) {
        if (!StringUtils.hasText(sessionId)) {
            throw new IllegalArgumentException("sessionId不能为空");
        }
        SessionContext context = sessions.get(sessionId.trim());
        if (context == null) {
            throw new IllegalArgumentException("巡检会话不存在");
        }
        return context;
    }

    private void validateMode(String mode) {
        String normalized = String.valueOf(mode).trim();
        if (!Set.of("0", "1", "2").contains(normalized)) {
            throw new IllegalArgumentException("无效的巡检模式");
        }
    }

    private String toRelativeStoragePath(Path absolutePath) {
        Path root = resolveStoragePath(".");
        Path normalized = absolutePath.toAbsolutePath().normalize();
        if (!normalized.startsWith(root)) {
            throw new IllegalArgumentException("非法存储路径");
        }
        return root.relativize(normalized).toString().replace('\\', '/');
    }

    private Path resolveStoragePath(String relativePath) {
        Path root = Paths.get(storageProperties.getRootDir()).toAbsolutePath().normalize();
        Path path = root.resolve(relativePath).normalize();
        if (!path.startsWith(root)) {
            throw new IllegalArgumentException("非法存储路径");
        }
        return path;
    }

    private List<Path> listFiles(Path directory) throws IOException {
        if (!Files.exists(directory) || !Files.isDirectory(directory)) {
            return Collections.emptyList();
        }

        try (Stream<Path> stream = Files.list(directory)) {
            return stream
                .filter(Files::isRegularFile)
                .sorted(Comparator.comparing(path -> path.getFileName().toString()))
                .toList();
        }
    }

    private String getResultsDir() {
        return StringUtils.hasText(storageProperties.getResultsDir()) ? storageProperties.getResultsDir().trim() : "results";
    }

    private String getSessionsDir() {
        return StringUtils.hasText(storageProperties.getSessionsDir()) ? storageProperties.getSessionsDir().trim() : ".sessions";
    }

    private String getVideoDir() {
        return StringUtils.hasText(storageProperties.getVideoDir()) ? storageProperties.getVideoDir().trim() : "videos";
    }

    private String getImageDir() {
        return StringUtils.hasText(storageProperties.getImageDir()) ? storageProperties.getImageDir().trim() : "images";
    }

    private String getPointsDir() {
        return StringUtils.hasText(storageProperties.getPointsDir()) ? storageProperties.getPointsDir().trim() : "points";
    }

    private void deleteRecursively(Path directory) throws IOException {
        if (directory == null || !Files.exists(directory)) {
            return;
        }

        try (Stream<Path> stream = Files.walk(directory)) {
            stream.sorted(Comparator.reverseOrder()).forEach(path -> {
                try {
                    Files.deleteIfExists(path);
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

    private static class SessionContext {
        private String sessionId;
        private String mode;
        private String environment;
        private String operatorName;
        private LocalDateTime startedAt;
        private Path sessionDir;
        private Path captureDir;
        private Path tempVideoPath;
        private Process recorderProcess;
        private int captureSequence;
    }
}
