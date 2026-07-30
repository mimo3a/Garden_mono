package com.example.garden.service;

import com.example.garden.model.Measurement;
import com.example.garden.model.Sensor;
import com.example.garden.repository.MeasurementRepository;
import com.example.garden.repository.SensorRepository;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.springframework.messaging.Message;
import org.springframework.messaging.MessageHandler;
import org.springframework.stereotype.Service;

@Service
public class MqttMessageHandler implements MessageHandler {

    private final SensorRepository sensorRepository;
    private final MeasurementRepository measurementRepository;
    private final ObjectMapper objectMapper = new ObjectMapper();

    public MqttMessageHandler(SensorRepository sensorRepository,
                              MeasurementRepository measurementRepository) {
        this.sensorRepository = sensorRepository;
        this.measurementRepository = measurementRepository;
    }

    @Override
    public void handleMessage(Message<?> message) {
        try {
            String topic = message.getHeaders()
                    .get("mqtt_receivedTopic")
                    .toString();

            String[] parts = topic.split("/");
            Integer deviceId = Integer.parseInt(parts[1]);

            String payload = message.getPayload().toString();
            JsonNode json = objectMapper.readTree(payload);

            JsonNode tempNode = json.get("temperature");
            JsonNode soilArray = json.get("soil");
            JsonNode batteryNode = json.get("battery");

            Sensor sensor = sensorRepository.findByDeviceId(deviceId)
                    .orElseGet(() -> {
                        Sensor s = new Sensor();
                        s.setDeviceId(deviceId);
                        s.setName("Group " + deviceId);
                        return sensorRepository.save(s);
                    });

            if (tempNode != null && !tempNode.isNull()) {
                saveMeasurement(sensor, "temperature", tempNode.asDouble());
            }

            for (int i = 0; i < soilArray.size(); i++) {
                saveMeasurement(sensor, "soil" + (i + 1),
                        soilArray.get(i).asDouble());
            }

            if (batteryNode != null && !batteryNode.isNull()) {
                saveMeasurement(sensor, "battery", batteryNode.asDouble());
            }

            System.out.println("Saved data from device " + deviceId);

        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void saveMeasurement(Sensor sensor, String type, Double value) {
        Measurement m = new Measurement();
        m.setSensor(sensor);
        m.setType(type);
        m.setValue(value);
        measurementRepository.save(m);
    }
}