package com.example.garden.config;

import com.example.garden.repository.AppUserRepository;
import com.example.garden.security.AppUserPrincipal;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.HttpMethod;
import org.springframework.security.authentication.AuthenticationManager;
import org.springframework.security.authentication.dao.DaoAuthenticationProvider;
import org.springframework.security.config.annotation.authentication.configuration.AuthenticationConfiguration;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.annotation.web.configuration.EnableWebSecurity;
import org.springframework.security.config.http.SessionCreationPolicy;
import org.springframework.security.core.userdetails.UserDetailsService;
import org.springframework.security.crypto.factory.PasswordEncoderFactories;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.security.web.SecurityFilterChain;
import org.springframework.security.web.csrf.CookieCsrfTokenRepository;
import org.springframework.security.config.Customizer;

@Configuration
@EnableWebSecurity
public class SecurityConfig {
    @Bean
    UserDetailsService userDetailsService(AppUserRepository users) {
        return email -> users.findByEmailIgnoreCase(email).map(AppUserPrincipal::from)
                .orElseThrow(() -> new org.springframework.security.core.userdetails.UsernameNotFoundException("Invalid email or password"));
    }

    @Bean PasswordEncoder passwordEncoder() { return PasswordEncoderFactories.createDelegatingPasswordEncoder(); }

    @Bean
    DaoAuthenticationProvider authenticationProvider(UserDetailsService users, PasswordEncoder encoder) {
        DaoAuthenticationProvider provider = new DaoAuthenticationProvider(users);
        provider.setPasswordEncoder(encoder);
        return provider;
    }

    @Bean AuthenticationManager authenticationManager(AuthenticationConfiguration configuration) throws Exception { return configuration.getAuthenticationManager(); }

    @Bean
    SecurityFilterChain securityFilterChain(HttpSecurity http, DaoAuthenticationProvider provider,
                                            @Value("${app.security.cookie-secure:true}") boolean cookieSecure) throws Exception {
        CookieCsrfTokenRepository csrf = CookieCsrfTokenRepository.withHttpOnlyFalse();
        csrf.setCookieCustomizer(cookie -> cookie.secure(cookieSecure).sameSite("Strict"));
        return http.authenticationProvider(provider).cors(Customizer.withDefaults())
                .csrf(config -> config.csrfTokenRepository(csrf).ignoringRequestMatchers("/api/auth/csrf"))
                .sessionManagement(config -> config.sessionCreationPolicy(SessionCreationPolicy.IF_REQUIRED))
                .authorizeHttpRequests(auth -> auth
                        .requestMatchers(HttpMethod.GET, "/api/auth/csrf").permitAll()
                        .requestMatchers(HttpMethod.POST, "/api/auth/login", "/api/auth/accept-invitation").permitAll()
                        .requestMatchers("/api/admin/**").hasRole("ADMIN")
                        .requestMatchers("/api/**").authenticated().anyRequest().permitAll())
                .formLogin(config -> config.disable()).httpBasic(config -> config.disable()).build();
    }
}
