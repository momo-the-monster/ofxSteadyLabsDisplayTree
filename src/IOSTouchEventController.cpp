/***********************************************************************
 
 Copyright (c) 2011,2012, Mike Manh
 ***STEADY LTD http://steadyltd.com ***
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without 
 modification, are permitted provided that the following conditions are met:
 
 *Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.
 *Redistributions in binary form must reproduce the above copyright notice, 
 this list of conditions and the following disclaimer in the 
 documentation and/or other materials provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/

#include <iostream>

#include "IOSTouchEventController.h"
#include "DisplayObject.h"

//can we just instantiate this once here?
IOSTouchEventController* IOSTouchEventController::instance = new IOSTouchEventController();

//static strings

IOSTouchEventController::IOSTouchEventController(){
}

void IOSTouchEventController::init(){
    instance->_init();
}

void IOSTouchEventController::_init(){
    //worldBlockingState = NONE;
    ofRegisterTouchEvents(this);
}


void IOSTouchEventController::processEvents(){
    //todo, go through the queue and process the events
    //possibly do some kind of mutex locking and unlocking to prevent race conditions
    
    instance->_sort();//in case something got moved to the top of the deck...
    instance->_processEvents();
}

struct more_than_renderOrder
{
    inline bool operator() ( IOSTouchEnabler* obj1, IOSTouchEnabler* obj2)
    {
        return ( obj1->getTarget()->renderOrder > obj2->getTarget()->renderOrder );
    }
};

void IOSTouchEventController::_sort(){
    std::sort( _touchEnablers.begin(), _touchEnablers.end(), more_than_renderOrder() );
    
    // cout << "_sort::order::" <<endl;
    for( int i = 0; i < _touchEnablers.size(); i++ ){
        // cout << _touchEnablers[ i ]->getTarget()->renderOrder << " " << _touchEnablers[ i ]->getTarget()->name <<",";
    }
    // cout <<endl;
}

void IOSTouchEventController::_processEvents()
{
    TouchEvent* curEvent;
    
    while( _eventQueue.size() > 0 ){
        curEvent = _eventQueue.front();
        _eventQueue.pop(); //seriously, you can't get the reference without handling it?        
        _handleEvent( curEvent );   
        
        delete curEvent;
    }
    
    
    if( _eventQueue.size() > 0){
        curEvent = new TouchEvent();
        curEvent->args = ofTouchEventArgs();
        
        TouchEvent *lastEvent = _eventQueue.back();
        curEvent->args.x = lastEvent->args.x;
        curEvent->args.x = lastEvent->args.y;
        
        curEvent->type = STILL;
        
        _handleEvent(curEvent);
        delete curEvent;
    }
}

void IOSTouchEventController::_handleEvent( TouchEvent* inEvent){
    
   
    int touchX = inEvent->args.x;
    int touchY = inEvent->args.y;
    BlockingState curBlockingState;
    
    switch( inEvent->type ){
        case TOUCH_DOWN:
            
            for ( int i = 0; i < _touchEnablers.size(); i++ )
            {
                curBlockingState = _touchEnablers[ i ]->blockingState;
                
                if ( _touchEnablers[ i ]->getTarget()->hitTest(touchX, touchY) ){
                    
                    _touchEnablers[ i ]->_touchDown(inEvent->args, false);
                    
                    //worldBlockingState = _touchEnablers[ i ]->blockingState;
                    
                    break;
                    //if ( _touchEnablers[ i ]->blockingState == BELOW ) break;
                }
            }
            
            break;
            
        case TOUCH_CANCELLED:
        case TOUCH_UP:

            for ( int i = 0; i < _touchEnablers.size(); i++ )
            {
                curBlockingState = _touchEnablers[ i ]->blockingState;
                
                if ( curBlockingState == ALL) continue;
                
                if ( curBlockingState == ENGAGED ) {
                    _touchEnablers[ i ]->_touchUp(inEvent->args, true);
                    //worldBlockingState = NONE;
                    continue;
                }
                
                _touchEnablers[ i ]->_touchUp(inEvent->args, true);
                
                if ( curBlockingState == BELOW ) break;
            }
            
            break;
            
        case STILL:
        case TOUCH_MOVE:
            
            for ( int i = 0; i < _touchEnablers.size(); i++ )
            {
                curBlockingState = _touchEnablers[ i ]->blockingState;
                
                //if ( curBlockingState == ALL) break;
                
                if ( curBlockingState == ENGAGED ) _touchEnablers[ i ]->_touchMoved(inEvent->args);
                /*
                 curBlockingState = _touchEnablers[ i ]->blockingState;
                 
                 if ( _touchEnablers[ i ]->_touchMoved(inEvent->args) ){
                 if ( worldBlockingState == ALL) break;
                 if ( curBlockingState == ALL || curBlockingState == ENGAGED) break;
                 if ( curBlockingState == BELOW ){
                 for( i = i+1; i < _touchEnablers.size(); i++ ){
                 _touchEnablers[ i ]->_touchMovedBlocked(inEvent->args);
                 }
                 }
                 }
                 */
            }
            break;
            
        case TOUCH_DOUBLETAP:
            //Is this a generic view based doubletap or a specific target double tap? Target for now            
            for ( int i = 0; i < _touchEnablers.size(); i++ ){
                if ( _touchEnablers[ i ]->getTarget()->hitTest(touchX, touchY) ){
                    _touchEnablers[ i ]->_touchDoubleTap(inEvent->args, true);
                    if ( _touchEnablers[ i ]->blockingState == BELOW ){
                        break;//if it's a blocking touch event, then stop sending click events to things below
                    }
                }
            }
            
            break;
    }

}

void IOSTouchEventController::addEnabler(IOSTouchEnabler* inEnabler){
    IOSTouchEventController::instance->_addEnabler( inEnabler );
}

void IOSTouchEventController::removeEnabler(IOSTouchEnabler* inEnabler){
    IOSTouchEventController::instance->_removeEnabler( inEnabler );
}

void IOSTouchEventController::_addEnabler(IOSTouchEnabler* inEnabler){
    
    //look to make sure it's not already added
    map<IOSTouchEnabler*, int>::const_iterator iter = _touchEnablerToIndex.find(inEnabler);
    if ( iter == _touchEnablerToIndex.end() ){
        //it's not in there, go ahead and add it
        _touchEnablers.push_back(inEnabler);
        _touchEnablerToIndex[ inEnabler ] = _touchEnablers.size(); //for now, until we sort it again
    }
    else{
        //cout << inEnabler->_target->name << " IOSTouchEventController::addEnabler::warning, trying to add an enabler that's already here\n";
        printf("IOSTouchEventController::_addEnabler : inEnabler :: WARNING, trying to add an enabler that's already here\n");
    }
}

void IOSTouchEventController::_removeEnabler(IOSTouchEnabler* inEnabler){
    map<IOSTouchEnabler*, int>::iterator iter = _touchEnablerToIndex.find(inEnabler);
    int index = 0;
    if ( iter != _touchEnablerToIndex.end() ){
        
        //it's there
        index = _touchEnablerToIndex[ inEnabler ];
        
        _touchEnablerToIndex.erase( iter );
        
    }
    else{
        
        if ( _touchEnablers[ index ] != inEnabler ){ //index is the correct one for that enabler
            //find the correct index
            index = 0;
            while ( index < _touchEnablers.size() && _touchEnablers[index] != inEnabler ){
                index++;
            }
        }
        if( index < _touchEnablers.size() ){
            //delete it from touchEnablers
            _touchEnablers.erase( _touchEnablers.begin() + index );
        }
    }   
}
//---------------------------------------------------------------------------------------------

void IOSTouchEventController::touchDown( ofTouchEventArgs &touch )
{
    TouchEvent *savedEvent = new TouchEvent();
    savedEvent->type = TOUCH_DOWN;
    savedEvent->args = touch;
    _eventQueue.push(savedEvent);
}

void IOSTouchEventController::touchUp( ofTouchEventArgs &touch )
{
    //printf("IOSTouchEventController::touchUp ::::::::::::::::::: \n");
    
    TouchEvent *savedEvent = new TouchEvent();
    savedEvent->type = TOUCH_UP;
    savedEvent->args = touch;
    _eventQueue.push(savedEvent);
}

void IOSTouchEventController::touchMoved( ofTouchEventArgs &touch )
{
    TouchEvent *savedEvent = new TouchEvent();
    savedEvent->type = TOUCH_MOVE;
    savedEvent->args = touch;
    _eventQueue.push(savedEvent);
}

void IOSTouchEventController::touchCancelled( ofTouchEventArgs &touch )
{
    TouchEvent *savedEvent = new TouchEvent();
    savedEvent->type = TOUCH_UP;
    savedEvent->args = touch;
    _eventQueue.push(savedEvent);
}

void IOSTouchEventController::touchDoubleTap( ofTouchEventArgs &touch )
{
    TouchEvent *savedEvent = new TouchEvent();
    savedEvent->type = TOUCH_DOUBLETAP;
    savedEvent->args = touch;
    _eventQueue.push(savedEvent);
}
