package com.hitzri.pipelinerobot.config;

import com.hitzri.pipelinerobot.security.JwtAuthenticationFilter;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.util.Map;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.Customizer;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.annotation.web.configuration.EnableWebSecurity;
import org.springframework.security.config.http.SessionCreationPolicy;
import org.springframework.security.core.Authentication;
import org.springframework.security.web.SecurityFilterChain;
import org.springframework.security.web.authentication.UsernamePasswordAuthenticationFilter;
import org.springframework.web.cors.CorsConfiguration;
import org.springframework.web.cors.CorsConfigurationSource;
import org.springframework.web.cors.UrlBasedCorsConfigurationSource;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

@Configuration
@EnableWebSecurity
public class SecurityConfig {

    private static final Logger log = LoggerFactory.getLogger(SecurityConfig.class);

    private final JwtAuthenticationFilter jwtAuthenticationFilter;
    private final CorsProperties corsProperties;
    private final ObjectMapper objectMapper;

    public SecurityConfig(
        JwtAuthenticationFilter jwtAuthenticationFilter,
        CorsProperties corsProperties,
        ObjectMapper objectMapper
    ) {
        this.jwtAuthenticationFilter = jwtAuthenticationFilter;
        this.corsProperties = corsProperties;
        this.objectMapper = objectMapper;
    }

    @Bean
    public SecurityFilterChain securityFilterChain(HttpSecurity http) throws Exception {
        http
            .csrf(csrf -> csrf.disable())
            .cors(Customizer.withDefaults())
            .sessionManagement(session -> session.sessionCreationPolicy(SessionCreationPolicy.STATELESS))
            .requestCache(requestCache -> requestCache.disable())
            .anonymous(Customizer.withDefaults())
            .exceptionHandling(exceptionHandling -> exceptionHandling
                .authenticationEntryPoint((request, response, exception) -> {
                    log.warn(
                        "Security authentication failure: method={}, uri={}, message={}",
                        request.getMethod(),
                        request.getRequestURI(),
                        exception.getMessage()
                    );
                    writeSecurityError(response, HttpStatus.UNAUTHORIZED, "未登录或登录态已失效");
                })
                .accessDeniedHandler((request, response, exception) -> {
                    Authentication authentication = org.springframework.security.core.context.SecurityContextHolder
                        .getContext()
                        .getAuthentication();
                    log.warn(
                        "Security access denied: method={}, uri={}, authenticated={}, principal={}, message={}",
                        request.getMethod(),
                        request.getRequestURI(),
                        authentication != null,
                        authentication != null ? authentication.getName() : "<anonymous>",
                        exception.getMessage()
                    );
                    writeSecurityError(response, HttpStatus.FORBIDDEN, "接口权限被拒绝");
                })
            )
            .authorizeHttpRequests(authorize -> authorize
                .requestMatchers("/auth/**", "/files/**", "/ota/files/**", "/error").permitAll()
                .requestMatchers("/inspection/**").authenticated()
                .anyRequest().authenticated()
            )
            .addFilterBefore(jwtAuthenticationFilter, UsernamePasswordAuthenticationFilter.class);

        return http.build();
    }

    @Bean
    public CorsConfigurationSource corsConfigurationSource() {
        CorsConfiguration configuration = new CorsConfiguration();
        configuration.setAllowCredentials(false);
        configuration.setAllowedOrigins(corsProperties.getAllowedOriginList());
        configuration.addAllowedHeader("*");
        configuration.addAllowedMethod("*");
        configuration.addExposedHeader("Content-Disposition");
        UrlBasedCorsConfigurationSource source = new UrlBasedCorsConfigurationSource();
        source.registerCorsConfiguration("/**", configuration);
        return source;
    }

    private void writeSecurityError(
        jakarta.servlet.http.HttpServletResponse response,
        HttpStatus status,
        String message
    ) throws java.io.IOException {
        response.setStatus(status.value());
        response.setCharacterEncoding("UTF-8");
        response.setContentType(MediaType.APPLICATION_JSON_VALUE);
        objectMapper.writeValue(
            response.getWriter(),
            Map.of(
                "message", message,
                "status", status.value()
            )
        );
    }
}
