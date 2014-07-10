



#include <iostream>


#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkCollectionIterator.h>

#include <SPINEContoursReader.h>
#include <spinecontoursinterpolation.h>
#include <vtkProperty.h>
#include <vtkIterativeClosestPointTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkLandmarkTransform.h>
#include <vtkMatrix4x4.h>
#include <vtkPointLocator.h>
#include <vtkCellArray.h>
#include <vtkDoubleArray.h>

#include <spinecirclecenter.h>
#include <vnl/algo/vnl_levenberg_marquardt.h>
#include <minimizecircledistance.h>
#include <vtkMath.h>

using namespace std;

int main(int argv, char** argc){

    cout<<argc[1]<<endl;

    vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
    vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(renderer);
    vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    renderWindowInteractor->SetRenderWindow(renderWindow);
    vtkInteractorStyleTrackballCamera *style = vtkInteractorStyleTrackballCamera::New();
    renderWindowInteractor->SetInteractorStyle(style);
    renderer->SetBackground(.3, .6, .3); // Background color green


    vtkSmartPointer<SPINEContoursReader> sourcereader = vtkSmartPointer<SPINEContoursReader>::New();
    sourcereader->SetFileName(argc[1]);
    //sourcereader->DebugOn();

    sourcereader->Update();

    vtkPolyDataCollection* contours = sourcereader->GetOutput();
    vtkCollectionSimpleIterator it;
    contours->InitTraversal(it);

    vtkSmartPointer<vtkPolyData> target = 0;
    double contourlength = 0;
    int numsamples = 200;


    vector<vtkPolyData*> vectorsource;

    for(unsigned i = 0; i < contours->GetNumberOfItems(); i++){

        vtkPolyData* nextpoly = contours->GetNextPolyData(it);

        vtkSmartPointer<SPINEContoursInterpolation> contourinterpolation = vtkSmartPointer<SPINEContoursInterpolation>::New();
        contourinterpolation->SetInputData(nextpoly);
        contourinterpolation->Update();

        cout<<"Contour length = "<<contourinterpolation->GetContourLength()<<endl;
        cout<<"Num points = "<<contourinterpolation->GetOutput()->GetNumberOfPoints()<<endl;

        vectorsource.push_back(contourinterpolation->GetOutput());


        vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputData(contourinterpolation->GetOutput());

        vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);

        renderer->AddActor(actor);

        if(contourlength < contourinterpolation->GetContourLength()){
            target = contourinterpolation->GetOutput();
            contourlength = contourinterpolation->GetContourLength();
        }
    }

    for(unsigned i = 0; i < vectorsource.size(); i++){
        if(vectorsource[i] != target){

            vtkSmartPointer<vtkIterativeClosestPointTransform> icp = vtkSmartPointer<vtkIterativeClosestPointTransform>::New();
            icp->SetSource(vectorsource[i]);
            icp->SetTarget(target);
            icp->GetLandmarkTransform()->SetModeToSimilarity();
            icp->SetMaximumNumberOfIterations(10);
            icp->StartByMatchingCentroidsOn();
            icp->Modified();
            icp->Update();

            // Get the resulting transformation matrix (this matrix takes the source points to the target points)
            vtkSmartPointer<vtkMatrix4x4> m = icp->GetMatrix();
            //std::cout << "The resulting matrix is: " << *m << std::endl;

            vtkSmartPointer<vtkTransformPolyDataFilter> icpTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
            icpTransformFilter->SetInputData(vectorsource[i]);
            icpTransformFilter->SetTransform(icp);
            icpTransformFilter->Update();

            vtkPolyData* source = icpTransformFilter->GetOutput();
            int angle = 0;
            double minangle = 0;
            double minDistance = DBL_MAX;

            while(angle < 360){
                double distance = 0;
                for(unsigned i = 0; i < source->GetNumberOfPoints(); i++){
                    int sourceindex = round(angle/360.0*source->GetNumberOfPoints() + i);
                    if(sourceindex > source->GetNumberOfPoints()-1){
                        sourceindex -= source->GetNumberOfPoints();
                    }
                    int targetindex = round(i/((double)source->GetNumberOfPoints()-1.0) * (target->GetNumberOfPoints()-1));

                    double ps[3], pt[3];
                    source->GetPoint(sourceindex, ps);
                    target->GetPoint(targetindex, pt);

                    distance += sqrt(vtkMath::Distance2BetweenPoints(pt, ps));
                }

                if(distance < minDistance){
                    minangle = angle;
                    minDistance = distance;
                }
                angle++;

            }


            /*MinimizeCircleDistance mindist;
            mindist.SetSource(source);
            mindist.SetTarget(target);

            vnl_levenberg_marquardt levenberg(mindist);

            vnl_vector<double> anglevnl(1);
            anglevnl.fill(0);

            levenberg.minimize(anglevnl);
            double angle = anglevnl[0];

            while(angle < 0){
                angle += 2*M_PI;
            }

            double delta = angle/(2*M_PI) * source->GetNumberOfPoints();*/
            double stepj = source->GetNumberOfPoints()/numsamples;

            vtkSmartPointer<vtkPoints> connectingpoints = vtkSmartPointer<vtkPoints>::New();
            vtkSmartPointer<vtkCellArray> connectingcellarray = vtkSmartPointer<vtkCellArray>::New();

            for(double j = 0; j < source->GetNumberOfPoints(); j+=stepj){
                int sourceindex = round(minangle/360.0*source->GetNumberOfPoints() + j);
                if(sourceindex > source->GetNumberOfPoints() - 1){
                    sourceindex -= source->GetNumberOfPoints();
                }

                int targetindex = round(j/((double)source->GetNumberOfPoints()-1) * (target->GetNumberOfPoints()-1));
                if(targetindex >= target->GetNumberOfPoints()){
                    targetindex = 0;
                }
                double ps[3], pt[3];
                source->GetPoint(sourceindex, ps);
                target->GetPoint(targetindex, pt);
                vtkIdType id0 = connectingpoints->InsertNextPoint(ps[0], ps[1], ps[2]);
                vtkIdType id1 = connectingpoints->InsertNextPoint(pt[0], pt[1], pt[2]);

                vtkSmartPointer<vtkLine> line = vtkSmartPointer<vtkLine>::New();
                line->GetPointIds()->SetId(0, id0);
                line->GetPointIds()->SetId(1, id1);

                connectingcellarray->InsertNextCell(line);
            }

            vtkSmartPointer<vtkPolyData> connectingpoly = vtkSmartPointer<vtkPolyData>::New();
            connectingpoly->SetPoints(connectingpoints);
            connectingpoly->SetLines(connectingcellarray);


            vtkSmartPointer<vtkPolyDataMapper> connectingmapper = vtkSmartPointer<vtkPolyDataMapper>::New();
            connectingmapper->SetInputData(connectingpoly);

            vtkSmartPointer<vtkActor> connectingactor = vtkSmartPointer<vtkActor>::New();
            connectingactor->SetMapper(connectingmapper);
            connectingactor->GetProperty()->SetColor(0,255,0);

            renderer->AddActor(connectingactor);


        }
    }
    /*for(unsigned i = 0; i < vectorsource.size(); i++){
        if(vectorsource[i] != target){

                    vtkSmartPointer<vtkIterativeClosestPointTransform> icp = vtkSmartPointer<vtkIterativeClosestPointTransform>::New();
                    icp->SetSource(vectorsource[i]);
                    icp->SetTarget(target);
                    icp->GetLandmarkTransform()->SetModeToSimilarity();
                    icp->SetMaximumNumberOfIterations(10);
                    icp->StartByMatchingCentroidsOn();
                    icp->Modified();
                    icp->Update();

                    // Get the resulting transformation matrix (this matrix takes the source points to the target points)
                    vtkSmartPointer<vtkMatrix4x4> m = icp->GetMatrix();
                    //std::cout << "The resulting matrix is: " << *m << std::endl;

                    vtkSmartPointer<vtkTransformPolyDataFilter> icpTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
                    icpTransformFilter->SetInputData(vectorsource[i]);
                    icpTransformFilter->SetTransform(icp);
                    icpTransformFilter->Update();

                    vtkPolyData* source = icpTransformFilter->GetOutput();
                    vtkSmartPointer<vtkPointLocator> sourcepointlocator = vtkSmartPointer<vtkPointLocator>::New();
                    sourcepointlocator->SetDataSet(source);
                    sourcepointlocator->BuildLocator();


                    vtkSmartPointer<vtkPoints> samplepoints = vtkSmartPointer<vtkPoints>::New();
                    vtkSmartPointer<vtkPoints> connectingpoints = vtkSmartPointer<vtkPoints>::New();

                    for(unsigned i = 0; i < target->GetNumberOfPoints(); i+=stepi){
                        double p[3];
                        target->GetPoint(i, p);

                        connectingpoints->InsertNextPoint(p[0], p[1], p[2]);

                        vtkIdType ids = sourcepointlocator->FindClosestPoint(p);
                        source->GetPoint(ids, p);
                        samplepoints->InsertNextPoint(p[0], p[1], p[2]);

                        connectingpoints->InsertNextPoint(p[0], p[1], p[2]);
                    }

                    vtkSmartPointer<vtkCellArray> samplecellarray = vtkSmartPointer<vtkCellArray>::New();
                    for(unsigned i = 0, j = 0; i < samplepoints->GetNumberOfPoints(); i++, j+=stepi){
                        vtkSmartPointer<vtkLine> line = vtkSmartPointer<vtkLine>::New();
                        line->GetPointIds()->SetId(0, i);

                        double p0[3], p1[3];
                        samplepoints->GetPoint(i, p0);
                        if(i == samplepoints->GetNumberOfPoints() - 1){
                          line->GetPointIds()->SetId(1, 0);
                          samplepoints->GetPoint(0, p1);
                        }else{
                            line->GetPointIds()->SetId(1, i+1);
                            samplepoints->GetPoint(i+1, p1);
                        }

                        samplecellarray->InsertNextCell(line);
                    }

                    vtkSmartPointer<vtkPolyData> samplepoly = vtkSmartPointer<vtkPolyData>::New();
                    samplepoly->SetPoints(samplepoints);
                    samplepoly->SetLines(samplecellarray);


                    vtkSmartPointer<vtkPolyDataMapper> mapperreg = vtkSmartPointer<vtkPolyDataMapper>::New();
                    mapperreg->SetInputData(source);

                    vtkSmartPointer<vtkActor> actorreg = vtkSmartPointer<vtkActor>::New();
                    actorreg->SetMapper(mapperreg);
                    actorreg->GetProperty()->SetColor(255,0,255);

                    renderer->AddActor(actorreg);



                    vtkSmartPointer<vtkPolyDataMapper> samplemapper = vtkSmartPointer<vtkPolyDataMapper>::New();
                    samplemapper->SetInputData(samplepoly);

                    cout<<"Sample points= "<<samplepoly->GetNumberOfPoints()<<endl;

                    vtkSmartPointer<vtkActor> actorsample = vtkSmartPointer<vtkActor>::New();
                    actorsample->SetMapper(samplemapper);
                    actorsample->GetProperty()->SetColor(0,255,255);

                    renderer->AddActor(actorsample);


                    vtkSmartPointer<vtkCellArray> connectingcellarray = vtkSmartPointer<vtkCellArray>::New();
                    for(unsigned i = 0; i < connectingpoints->GetNumberOfPoints() - 1; i+=2){
                        vtkSmartPointer<vtkLine> line = vtkSmartPointer<vtkLine>::New();
                        line->GetPointIds()->SetId(0, i);
                        line->GetPointIds()->SetId(1, i+1);

                        connectingcellarray->InsertNextCell(line);
                    }

                    vtkSmartPointer<vtkPolyData> connectingpoly = vtkSmartPointer<vtkPolyData>::New();
                    connectingpoly->SetPoints(connectingpoints);
                    connectingpoly->SetLines(connectingcellarray);


                    vtkSmartPointer<vtkPolyDataMapper> connectingmapper = vtkSmartPointer<vtkPolyDataMapper>::New();
                    connectingmapper->SetInputData(connectingpoly);

                    vtkSmartPointer<vtkActor> connectingactor = vtkSmartPointer<vtkActor>::New();
                    connectingactor->SetMapper(connectingmapper);
                    connectingactor->GetProperty()->SetColor(0,255,0);

                    renderer->AddActor(connectingactor);
                }

    }*/

    renderWindow->Render();
    renderWindowInteractor->Start();

    return EXIT_SUCCESS;

}

