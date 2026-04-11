package com.hitzri.pipelinerobot.security;

import io.jsonwebtoken.Claims;
import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.util.List;
import org.springframework.http.HttpHeaders;
import org.springframework.security.authentication.UsernamePasswordAuthenticationToken;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

@Component
public class JwtAuthenticationFilter extends OncePerRequestFilter {

    private static final Logger log = LoggerFactory.getLogger(JwtAuthenticationFilter.class);

    private final JwtTokenProvider jwtTokenProvider;

    public JwtAuthenticationFilter(JwtTokenProvider jwtTokenProvider) {
        this.jwtTokenProvider = jwtTokenProvider;
    }

    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
        throws ServletException, IOException {
        String authorization = request.getHeader(HttpHeaders.AUTHORIZATION);
        boolean debugInspectionRequest = request.getRequestURI() != null && request.getRequestURI().startsWith("/api/inspection/");

        if (debugInspectionRequest) {
            log.info(
                "JWT filter request: method={}, uri={}, hasAuthorizationHeader={}",
                request.getMethod(),
                request.getRequestURI(),
                authorization != null && !authorization.isBlank()
            );
        }

        if (authorization != null && authorization.startsWith("Bearer ")) {
            String token = authorization.substring(7);
            try {
                Claims claims = jwtTokenProvider.parseToken(token);
                String username = claims.getSubject();
                String role = claims.get("role", String.class);
                UsernamePasswordAuthenticationToken authentication = new UsernamePasswordAuthenticationToken(
                    username,
                    null,
                    List.of(new SimpleGrantedAuthority("ROLE_" + role))
                );
                SecurityContextHolder.getContext().setAuthentication(authentication);
                if (debugInspectionRequest) {
                    log.info(
                        "JWT authentication success: method={}, uri={}, username={}, role={}",
                        request.getMethod(),
                        request.getRequestURI(),
                        username,
                        role
                    );
                }
            } catch (Exception ignored) {
                SecurityContextHolder.clearContext();
                if (debugInspectionRequest) {
                    log.warn(
                        "JWT authentication failed: method={}, uri={}, message={}",
                        request.getMethod(),
                        request.getRequestURI(),
                        ignored.getMessage()
                    );
                }
            }
        }

        if (debugInspectionRequest) {
            log.info(
                "JWT filter security context: method={}, uri={}, authenticated={}",
                request.getMethod(),
                request.getRequestURI(),
                SecurityContextHolder.getContext().getAuthentication() != null
            );
        }
        filterChain.doFilter(request, response);
    }
}
