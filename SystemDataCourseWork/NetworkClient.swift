//
//  NetworkClient.swift
//  SystemDataCourseWork
//
//  Created by Александр Плешаков on 08.12.2024.
//

import Foundation
import Network

final class NetworkClient: ObservableObject {
    private let host = "127.0.0.1"
    private let timePort: UInt32 = 8080
    private var timeServerConnection: NWConnection?
    
    var timeServerResponse: String?
    @Published var timeZone: String?
    @Published var durationOfWork: String?
    @Published var lastReceiveFromTimeServer: String?

    // Подключение к серверу
    private func connectToServer(host: String, port: UInt32) {
        timeServerConnection = NWConnection(host: NWEndpoint.Host(host), port: NWEndpoint.Port(rawValue: UInt16(port))!, using: .tcp)
        
        timeServerConnection?.stateUpdateHandler = { state in
            switch state {
            case .ready:
                print("Connected to \(host):\(port)")
                self.startReceiving()
            case .failed(let error):
                print("Connection failed: \(error)")
            default:
                break
            }
        }
        
        timeServerConnection?.start(queue: .global())
    }
    
    // Непрерывное прослушивание данных
    private func startReceiving() {
        timeServerConnection?.receive(minimumIncompleteLength: 1, maximumLength: 1024) { [weak self] data, _, isComplete, error in
            guard let self = self else { return }
            
            if let data = data, !data.isEmpty {
                let response = String(decoding: data, as: UTF8.self)
                print("Received: \(response)")
                
                DispatchQueue.main.async { [weak self] in
                    guard let self else { return }
                    timeServerResponse = response
                    (timeZone, durationOfWork, lastReceiveFromTimeServer) = parseServerResponse(timeServerResponse) ?? (nil, nil, nil)
                }
            } else if let error = error {
                print("Error receiving data: \(error)")
            }
            
            if isComplete {
                print("Connection closed by server")
                self.timeServerConnection?.cancel()
            } else {
                // Слушаем снова
                self.startReceiving()
            }
        }
    }
    
    func connectTimeServer() {
        connectToServer(host: host, port: timePort)
    }
    
    func disconnectTimeServer() {
        timeServerConnection?.cancel()
        timeServerConnection = nil
        print("Disconnected from server")
    }
    
    private func parseServerResponse(_ response: String?) -> (currentTimezone: String, sessionDuration: String, currentTime: String)? {
        guard let response else { return nil }
        let lines = response.components(separatedBy: "\n")
        
        guard lines.count >= 3 else {
            print("Invalid response format")
            return nil
        }
        
        let timezoneLine = lines[0]
        let sessionLine = lines[1]
        let timeLine = lines[2]
        
        let currentTimezone = timezoneLine.components(separatedBy: ": ").last ?? "Unknown"
        let sessionDuration = sessionLine.components(separatedBy: ": ").last ?? "Unknown"
        let currentTime = timeLine.components(separatedBy: ": ").last ?? "Unknown"
        
        return (currentTimezone: currentTimezone, sessionDuration: sessionDuration, currentTime: currentTime)
    }
}

