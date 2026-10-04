package com.example.garden.config;

import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.integration.config.EnableIntegration;
import org.springframework.integration.channel.DirectChannel;
import org.springframework.integration.core.MessageProducer;
import org.springframework.integration.mqtt.core.DefaultMqttPahoClientFactory;
import org.springframework.integration.mqtt.core.MqttPahoClientFactory;
import org.springframework.integration.mqtt.inbound.MqttPahoMessageDrivenChannelAdapter;
import org.springframework.messaging.MessageChannel;
import org.springframework.messaging.MessageHandler;
import org.springframework.integration.annotation.ServiceActivator;
import com.example.garden.service.MqttMessageHandler;

@Configuration
@EnableIntegration
public class MqttConfig {

   @Bean
public MqttPahoClientFactory mqttClientFactory(
        @Value("${mqtt.broker:tcp://mqtt:1883}") String broker,
        @Value("${mqtt.username}") String username,
        @Value("${mqtt.password}") String password) {
    DefaultMqttPahoClientFactory factory = new DefaultMqttPahoClientFactory();
    MqttConnectOptions options = new MqttConnectOptions();
    options.setServerURIs(new String[]{broker});
    options.setUserName(username);
    options.setPassword(password.toCharArray());
    options.setAutomaticReconnect(true);
    options.setCleanSession(false);
    factory.setConnectionOptions(options);
    return factory;
}

    @Bean
    public MessageChannel mqttInputChannel() {
        return new DirectChannel();
    }

    @Bean
    public MessageProducer inbound(MqttPahoClientFactory factory) {
        MqttPahoMessageDrivenChannelAdapter adapter =
        new MqttPahoMessageDrivenChannelAdapter(
                "garden-backend",
                factory,
                "smartgarden/+/data"
        );

        adapter.setQos(1);
        adapter.setOutputChannel(mqttInputChannel());
        return adapter;
    }

    @Bean
    @ServiceActivator(inputChannel = "mqttInputChannel")
    public MessageHandler handler(MqttMessageHandler handler) {
        return handler;
    }
}