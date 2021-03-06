#include <QObject>
#include "../vikNGS.h"
#include "../Variant.h"
#include "simulation/Simulation.h"

class AsyncJob : public QObject {
    Q_OBJECT

public:
    AsyncJob(){}
    ~AsyncJob(){}

public slots:
    void runVikngs() {
        try{
            Data result = startVikNGS(request);
            emit jobFinished(result, request.shouldPlot());
        }
        catch(...){
            Data empty;
            emit jobFinished(empty, false);
        }

        emit complete();
    }

    void runSimulation() {
        try{
            Data results = startSimulation(simRequest);
            emit simulationFinished(results, simRequest);
        }
        catch(...){
            Data empty;
            emit simulationFinished(empty, simRequest);
        }

        emit complete();
    }

    void setRequest(Request request){
        this->request = request;
    }
    void setSimulationRequest(SimulationRequest request){
        this->simRequest = request;
    }

signals:
    void jobFinished(Data, bool);
    void simulationFinished(Data, SimulationRequest);
    void complete();

private:
    Request request;
    SimulationRequest simRequest;
};

