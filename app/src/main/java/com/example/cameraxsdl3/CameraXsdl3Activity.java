/*
 * Program Name: CameraXSDL3 Activity
 * Package: com.example.cameraxsdl3
 * Description:
 * This Android activity integrates CameraX API with SDL (Simple DirectMedia Layer)
 * to capture and process YUV images from the device camera. It leverages JNI to
 * pass YUV data to native C functions for further processing and rendering.
 *
 * License: This software is provided 'as-is,' without any express or implied warranty.
 * Permission is granted for use, modification, and distribution under the stated terms.
 *
 * Author: Emmanuel Pinot
 * Email: manu.pinot@gmail.com
 * Year: 2024
 */

package com.example.cameraxsdl3;

import android.os.Bundle;
import android.util.Log;
import android.util.Size;

import androidx.annotation.NonNull;
import androidx.annotation.OptIn;
import androidx.camera.camera2.interop.ExperimentalCamera2Interop;
import androidx.camera.core.CameraSelector;
import androidx.camera.core.ImageAnalysis;
import androidx.camera.core.ImageProxy;
import androidx.camera.core.resolutionselector.ResolutionSelector;
import androidx.camera.core.resolutionselector.ResolutionStrategy;
import androidx.camera.lifecycle.ProcessCameraProvider;
import androidx.core.content.ContextCompat;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleOwner;
import androidx.lifecycle.LifecycleRegistry;

import com.google.common.util.concurrent.ListenableFuture;

import org.libsdl.app.SDLActivity;

import java.nio.ByteBuffer;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Main activity for CameraXSDL3 application. Extends SDLActivity and implements LifecycleOwner
 * for CameraX integration and image processing.
 */
public class CameraXsdl3Activity extends SDLActivity implements LifecycleOwner
{
    private LifecycleRegistry lifecycleRegistry; // Manages the lifecycle states
    private ExecutorService cameraExecutor;      // Executes camera tasks asynchronously
    private ProcessCameraProvider cameraProvider; // Provides camera access and control

    // Declare the native method to process YUV image data in C
    public native void processYUVImage(byte[] yuvData, int width, int height);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Initialize the lifecycle registry for managing lifecycle events
        lifecycleRegistry = new LifecycleRegistry(this);
        lifecycleRegistry.setCurrentState(Lifecycle.State.CREATED);

        // Initialize a single-threaded executor for handling camera tasks
        cameraExecutor = Executors.newSingleThreadExecutor();
    }

    /**
     * Starts the CameraX with the specified width and height for image resolution.
     *
     * @param width  The desired width for the camera feed.
     * @param height The desired height for the camera feed.
     */
    private void startCameraX(int width, int height) {
        // Get an instance of ProcessCameraProvider for camera control
        ListenableFuture<ProcessCameraProvider> cameraProviderFuture =
            ProcessCameraProvider.getInstance(this);

        // Add a listener to handle the future result asynchronously
        cameraProviderFuture.addListener(() -> {
            try {
                // Retrieve the camera provider instance
                cameraProvider = cameraProviderFuture.get();
                bindImageAnalysis(cameraProvider, width, height); // Bind the ImageAnalysis use case
            } catch (Exception e) {
                Log.e("CameraX", "Error binding camera provider", e);
            }
        }, ContextCompat.getMainExecutor(this));
    }

    /**
     * Binds ImageAnalysis use case to capture and process frames in the specified resolution.
     *
     * @param cameraProvider The camera provider instance.
     * @param width          The desired width for image analysis.
     * @param height         The desired height for image analysis.
     */
    @OptIn(markerClass = ExperimentalCamera2Interop.class)
    private void bindImageAnalysis(@NonNull ProcessCameraProvider cameraProvider, int width, int height) {
        // Set up a ResolutionSelector to specify resolution strategy
        ResolutionSelector resolutionSelector = new ResolutionSelector.Builder()
            .setResolutionStrategy(new ResolutionStrategy(new Size(width, height),
                ResolutionStrategy.FALLBACK_RULE_CLOSEST_LOWER_THEN_HIGHER))
            .build();

        // Configure ImageAnalysis with a resolution selector and backpressure strategy
        ImageAnalysis imageAnalysis = new ImageAnalysis.Builder()
            .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
            .setResolutionSelector(resolutionSelector)
            .build();

        // Set up an analyzer to process each frame asynchronously
        imageAnalysis.setAnalyzer(cameraExecutor, imageProxy -> {
            processImage(imageProxy);  // Perform image processing
            imageProxy.close();        // Close imageProxy to free resources
        });

        // Select the front-facing camera for analysis
        CameraSelector cameraSelector = CameraSelector.DEFAULT_FRONT_CAMERA;

        try {
            // Unbind any existing use cases before rebinding
            cameraProvider.unbindAll();

            // Bind ImageAnalysis use case to the lifecycle with the selected camera
            cameraProvider.bindToLifecycle(this, cameraSelector, imageAnalysis);

        } catch (Exception exc) {
            Log.e("CameraX", "CameraX Analysis use case binding failed", exc);
        }
    }

    /**
     * Processes the image captured from ImageAnalysis.
     *
     * @param image The ImageProxy object containing the image data.
     */
    private void processImage(ImageProxy image) {
        // Retrieve the Y, U, and V planes from the image
        ImageProxy.PlaneProxy[] planes = image.getPlanes();

        // Calculate total size needed for YUV data storage
        int totalSize = 0;
        for (ImageProxy.PlaneProxy plane : planes) {
            totalSize += plane.getBuffer().remaining();
        }

        // Create a byte array to hold the YUV data
        byte[] data = new byte[totalSize];

        // Copy data from each plane into the byte array
        int offset = 0;
        for (ImageProxy.PlaneProxy plane : planes) {
            ByteBuffer buffer = plane.getBuffer();
            int remaining = buffer.remaining();
            buffer.get(data, offset, remaining);
            offset += remaining;
        }

        // Pass the YUV data and dimensions to the native method for processing
        processYUVImage(data, image.getWidth(), image.getHeight());
    }

    @Override
    protected void onStart() {
        super.onStart();
        lifecycleRegistry.setCurrentState(Lifecycle.State.STARTED); // Update lifecycle state
    }

    @Override
    protected void onResume() {
        super.onResume();
        lifecycleRegistry.setCurrentState(Lifecycle.State.RESUMED); // Update lifecycle state
    }

    @Override
    protected void onPause() {
        super.onPause();
        lifecycleRegistry.setCurrentState(Lifecycle.State.STARTED); // Downgrade lifecycle state
    }

    @Override
    protected void onStop() {
        super.onStop();
        lifecycleRegistry.setCurrentState(Lifecycle.State.CREATED); // Downgrade lifecycle state
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        lifecycleRegistry.setCurrentState(Lifecycle.State.DESTROYED); // Set lifecycle to destroyed

        // Shut down the camera executor to free up resources
        cameraExecutor.shutdown();
    }

    @NonNull
    @Override
    public Lifecycle getLifecycle() {
        return lifecycleRegistry; // Return the lifecycle registry for CameraX lifecycle binding
    }
}
